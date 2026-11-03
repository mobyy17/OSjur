#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

// Layout virtual memory process sama seperti Ch.3: kode di 0x0, user stack di ujung 4 MiB pertama.
// 4 MiB = PROCESS_PAGE_FRAME_COUNT_MAX (1024) x PAGE_FRAME_SIZE (4 KiB).
#define PROCESS_USER_SPACE_SIZE 0x400000u
#define PROCESS_USER_STACK_TOP  (PROCESS_USER_SPACE_SIZE - 4)

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

// PID untuk process berikutnya. Naik terus dan tidak dipakai ulang, process pertama (shell) dapat PID 0.
static uint32_t next_pid = 0;

/**
 * Bebaskan semua frame user milik pcb, lalu page directory-nya.
 * Syarat: CR3 sedang menunjuk ke page directory pcb, karena paging_free_user_page_frame()
 * hanya bisa membaca page table milik page directory yang aktif.
 * Setelah fungsi ini selesai, CR3 sudah dipindah ke page directory kernel oleh paging_free_page_directory().
 *
 * @param pcb Process yang memorinya dibebaskan
 */
static void process_release_memory(struct ProcessControlBlock *pcb) {
    struct PageDirectory *page_dir = pcb->context.page_directory_virtual_addr;
    for (uint32_t i = 0; i < pcb->memory.page_frame_used_count; i++)
        paging_free_user_page_frame(page_dir, pcb->memory.virtual_addr_used[i]);
    pcb->memory.page_frame_used_count = 0;
    paging_free_page_directory(page_dir);
}

/**
 * Cari slot _process_list milik process aktif dengan PID tertentu.
 * PID dan slot sengaja dipisah: PID naik terus dan tidak dipakai ulang, sedangkan slot dipakai ulang.
 *
 * @param pid Process ID yang dicari
 * @return    Index slot, atau -1 kalau tidak ada process aktif dengan PID tersebut
 */
static int32_t process_get_slot_by_pid(uint32_t pid) {
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.active && _process_list[i].metadata.pid == pid)
            return i;
    }
    return -1;
}

struct ProcessControlBlock* process_get_current_running_pcb_pointer(void) {
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.active && _process_list[i].metadata.state == RUNNING)
            return &_process_list[i];
    }
    return NULL;
}

int32_t process_create_user_process(struct EXT2DriverRequest request) {
    // 0. Validasi: cari slot PCB kosong
    struct ProcessControlBlock *pcb = NULL;
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX && pcb == NULL; i++) {
        if (!_process_list[i].metadata.active)
            pcb = &_process_list[i];
    }
    if (pcb == NULL)
        return PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;

    // Entrypoint harus berada di 4 MiB pertama (bukan di kernel higher half)
    uint32_t entrypoint = (uint32_t) request.buf;
    if (entrypoint >= PROCESS_USER_SPACE_SIZE)
        return PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;

    // Executable harus muat sebelum ujung 4 MiB, dan memory fisik cukup untuk 1024 frame + 1 frame page table
    if (request.buffer_size > PROCESS_USER_SPACE_SIZE - entrypoint ||
        !paging_allocate_check((PROCESS_PAGE_FRAME_COUNT_MAX + 1) * PAGE_FRAME_SIZE))
        return PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;

    // request.name bisa menunjuk ke memori process pemanggil (syscall exec dari shell).
    // Setelah CR3 diganti, pointer itu tidak valid lagi, jadi nama disalin dulu ke stack kernel.
    char name[256]; // name_len bertipe uint8_t, maksimal 255 karakter
    memcpy(name, request.name, request.name_len);
    request.name = name;

    memset(pcb, 0, sizeof(struct ProcessControlBlock));

    // 1. Virtual address space baru
    struct PageDirectory *page_dir = paging_create_new_page_directory();
    if (page_dir == NULL)
        return PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
    pcb->context.page_directory_virtual_addr = page_dir;

    // 2. Alokasi frame dan load executable. Keduanya harus dilakukan saat page directory baru aktif,
    //    karena paging_get_page_table() hanya bekerja pada page directory yang ada di CR3.
    struct PageDirectory *prev_page_dir = paging_get_current_page_directory_addr();
    paging_use_page_directory(page_dir);

    for (uint32_t i = 0; i < PROCESS_PAGE_FRAME_COUNT_MAX; i++) {
        void *virtual_addr = (void*) (i * PAGE_FRAME_SIZE);
        if (!paging_allocate_user_page_frame(page_dir, virtual_addr)) {
            process_release_memory(pcb);
            paging_use_page_directory(prev_page_dir);
            return PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        }
        pcb->memory.virtual_addr_used[i] = virtual_addr;
        pcb->memory.page_frame_used_count++;
    }

    // Process dibatalkan kalau executable gagal dibaca, supaya process rusak tidak ikut dijalankan
    if (read(request) != 0) {
        process_release_memory(pcb);
        paging_use_page_directory(prev_page_dir);
        return PROCESS_CREATE_FAIL_FS_READ_FAILURE;
    }

    // Kembalikan CR3 seperti sebelum process creation
    paging_use_page_directory(prev_page_dir);

    // 3. Context awal. Register lain sudah 0 dari memset di atas.
    //    cs dan ss tidak disimpan di Context, keduanya diisi saat context switch (iret).
    uint32_t user_data_selector = GDT_USER_DATA_SEGMENT_SELECTOR | 0x3; // privilege level 3
    pcb->context.cpu.segment.ds = user_data_selector;
    pcb->context.cpu.segment.es = user_data_selector;
    pcb->context.cpu.segment.fs = user_data_selector;
    pcb->context.cpu.segment.gs = user_data_selector;
    pcb->context.cpu.stack.esp  = PROCESS_USER_STACK_TOP;
    pcb->context.eip            = entrypoint;
    pcb->context.eflags         = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    // 4. Metadata. Nama dipotong kalau lebih panjang dari PROCESS_NAME_LENGTH_MAX - 1,
    //    sisa array sudah 0 dari memset jadi tetap null-terminated.
    uint8_t name_len = request.name_len;
    if (name_len > PROCESS_NAME_LENGTH_MAX - 1)
        name_len = PROCESS_NAME_LENGTH_MAX - 1;
    memcpy(pcb->metadata.name, name, name_len);
    pcb->metadata.pid    = next_pid++;
    pcb->metadata.state  = READY;
    pcb->metadata.active = true;

    return PROCESS_CREATE_SUCCESS;
}

bool process_destroy(uint32_t pid) {
    // Shell adalah init process (PID 0) dan tidak boleh dimatikan
    if (pid == 0)
        return false;

    int32_t slot = process_get_slot_by_pid(pid);
    if (slot < 0)
        return false;
    struct ProcessControlBlock *pcb = &_process_list[slot];

    // Frame user hanya bisa dibebaskan saat page directory miliknya aktif
    struct PageDirectory *prev_page_dir = paging_get_current_page_directory_addr();
    struct PageDirectory *page_dir      = pcb->context.page_directory_virtual_addr;
    paging_use_page_directory(page_dir);
    process_release_memory(pcb);

    // Menghapus process lain (kill dari shell): kembali ke page directory pemanggil.
    // Menghapus diri sendiri (exit): CR3 tetap di page directory kernel, scheduler yang memilih process berikutnya.
    if (prev_page_dir != page_dir)
        paging_use_page_directory(prev_page_dir);

    memset(pcb, 0, sizeof(struct ProcessControlBlock)); // metadata.active jadi false, slot kosong lagi
    return true;
}

bool process_get_info(uint32_t slot, struct ProcessInfo *info) {
    if (slot >= PROCESS_COUNT_MAX || !_process_list[slot].metadata.active)
        return false;

    info->pid   = _process_list[slot].metadata.pid;
    info->state = _process_list[slot].metadata.state;
    memcpy(info->name, _process_list[slot].metadata.name, PROCESS_NAME_LENGTH_MAX);
    return true;
}