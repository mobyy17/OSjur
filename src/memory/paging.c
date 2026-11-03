#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"
#include "header/stdlib/string.h"

/**
 * _paging_kernel_page_directory and _paging_kernel_page_table is initialized on kernel-entrypoint.s
 */
__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory;

__attribute__((aligned(0x1000))) struct PageTable _paging_kernel_page_table;

/**
 * Global page frame manager state tracking free/busy frames.
 */
struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0 ... KERNEL_RESERVED_PAGE_FRAME_COUNT - 1] = true,
        [KERNEL_RESERVED_PAGE_FRAME_COUNT ... PAGE_FRAME_MAX_COUNT - 1] = false
    },
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - KERNEL_RESERVED_PAGE_FRAME_COUNT,
};

struct PageTable *paging_get_page_table(struct PageDirectory *page_dir, uint32_t page_directory_index) {
    //this helper function is given from kit but you can always modified it if you think theres a bug (spoiler alert) or you think theres a better approach

    if (page_directory_index == RECURSIVE_PAGE_DIRECTORY_INDEX || page_dir != paging_get_current_page_directory_addr()) return NULL;

    struct PageDirectoryEntry *dir_entry = &page_dir->table[page_directory_index];

    if (dir_entry->flag.present_bit) {
        return (struct PageTable *)(RECURSIVE_PAGE_TABLES_VIRTUAL_ADDRESS + page_directory_index * PAGE_FRAME_SIZE);
    }
    for (uint32_t page_frame_number = KERNEL_RESERVED_PAGE_FRAME_COUNT;
         page_frame_number < PAGE_FRAME_MAX_COUNT;
         page_frame_number++) {
        if (!page_manager_state.page_frame_map[page_frame_number]) {
            page_manager_state.page_frame_map[page_frame_number] = true;
            page_manager_state.free_page_frame_count--;
            dir_entry->flag = (struct PageDirectoryEntryFlag) {
                .present_bit = true,
                .write_bit = true,
                .us_bit = true,
            };
            dir_entry->page_table_base_address = page_frame_number;
            struct PageTable *page_table = (struct PageTable *)(RECURSIVE_PAGE_TABLES_VIRTUAL_ADDRESS + page_directory_index * PAGE_FRAME_SIZE);
            flush_single_tlb(page_table);
            memset(page_table, 0, PAGE_FRAME_SIZE);
            return page_table;
        }
    }
    return NULL;
}

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr,
    void *virtual_addr,
    struct PageTableEntryFlag flag
) {
    uint32_t page_directory_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    uint32_t page_table_index = ((uint32_t)virtual_addr >> 12) & 0x3FF;
    struct PageTable *page_table = paging_get_page_table(page_dir, page_directory_index);
    if (page_table == NULL) return;
    page_table->table[page_table_index].flag = flag;
    page_table->table[page_table_index].physical_page_base_address = ((uint32_t) physical_addr >> 12);
    flush_single_tlb(virtual_addr);
}

struct PageDirectory* paging_get_current_page_directory_addr(void) {
    uint32_t current_page_directory_phys_addr;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_page_directory_phys_addr): /* <Empty> */);
    uint32_t virtual_addr_page_dir = current_page_directory_phys_addr + KERNEL_VIRTUAL_ADDRESS_BASE;
    return (struct PageDirectory*) virtual_addr_page_dir;
}

void flush_single_tlb(void *virtual_addr) {
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr): "memory");
}



/* --- Memory Management --- */
bool paging_allocate_check(uint32_t amount) {
    // required_frames = ceil(amount / PAGE_FRAME_SIZE), ditulis begini supaya tidak overflow
    uint32_t required_frames = amount / PAGE_FRAME_SIZE + (amount % PAGE_FRAME_SIZE != 0);
    return required_frames <= page_manager_state.free_page_frame_count;
}


bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    // Virtual address 0xC0000000 ke atas milik kernel dan tidak boleh dipakai user
    if ((uint32_t) virtual_addr >= KERNEL_VIRTUAL_ADDRESS_BASE) return false;

    uint32_t page_directory_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    uint32_t page_table_index     = ((uint32_t) virtual_addr >> 12) & 0x3FF;

    // Page table diambil atau dibuat dulu, supaya bisa cek virtual page ini belum terpetakan.
    // Kalau sudah terpetakan lalu dialokasikan lagi, frame lama hilang dan tidak pernah di free.
    struct PageTable *page_table = paging_get_page_table(page_dir, page_directory_index);
    if (page_table == NULL || page_table->table[page_table_index].flag.present_bit) return false;

    // First-fit: frame kosong pertama setelah frame milik kernel
    for (uint32_t frame = KERNEL_RESERVED_PAGE_FRAME_COUNT; frame < PAGE_FRAME_MAX_COUNT; frame++) {
        if (!page_manager_state.page_frame_map[frame]) {
            page_manager_state.page_frame_map[frame] = true;
            page_manager_state.free_page_frame_count--;

            struct PageTableEntryFlag user_flag = {
                .present_bit = true,
                .write_bit   = true,
                .us_bit      = true,
            };
            update_page_directory_entry(page_dir, (void*) (frame * PAGE_FRAME_SIZE), virtual_addr, user_flag);

            // Frame bisa berisi sisa data pemakai sebelumnya, kosongkan dulu (penting untuk .bss program)
            memset((void*) ((uint32_t) virtual_addr & ~(PAGE_FRAME_SIZE - 1)), 0, PAGE_FRAME_SIZE);
            return true;
        }
    }
    return false;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    if ((uint32_t) virtual_addr >= KERNEL_VIRTUAL_ADDRESS_BASE) return false;

    uint32_t page_directory_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    uint32_t page_table_index     = ((uint32_t) virtual_addr >> 12) & 0x3FF;

    // Cek PDE dulu dimana paging_get_page_table() akan membuat page table baru kalau PDE belum ada
    if (!page_dir->table[page_directory_index].flag.present_bit) return false;
    struct PageTable *page_table = paging_get_page_table(page_dir, page_directory_index);
    if (page_table == NULL) return false;

    struct PageTableEntry *entry = &page_table->table[page_table_index];
    if (!entry->flag.present_bit) return false;

    uint32_t frame = entry->physical_page_base_address;
    page_manager_state.page_frame_map[frame] = false;
    page_manager_state.free_page_frame_count++;

    memset(entry, 0, sizeof(struct PageTableEntry));
    flush_single_tlb(virtual_addr);
    return true;
}


/* --- Process-related Memory Management --- */
__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT];

static struct {
    bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
} page_directory_manager;

struct PageDirectory* paging_create_new_page_directory(void) {
    for (uint32_t i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++) {
        if (page_directory_manager.page_directory_used[i]) continue;
        page_directory_manager.page_directory_used[i] = true;
        struct PageDirectory *page_dir = &page_directory_list[i];

        // Semua PDE higher half disalin supaya mapping kernel tambahan ikut terbawa ke process.
        for (uint32_t pde = 0x300; pde < RECURSIVE_PAGE_DIRECTORY_INDEX; pde++)
            page_dir->table[pde] = _paging_kernel_page_directory.table[pde];

        // Recursive mapping ke dirinya sendiri dan berlaku wajib untuk paging_get_page_table()
        // page_directory_list ada di kernel, sehingga physical = virtual - 0xC0000000
        uint32_t page_dir_physical_addr = (uint32_t) page_dir - KERNEL_VIRTUAL_ADDRESS_BASE;
        page_dir->table[RECURSIVE_PAGE_DIRECTORY_INDEX].flag = (struct PageDirectoryEntryFlag) {
            .present_bit = true,
            .write_bit   = true,
        };
        page_dir->table[RECURSIVE_PAGE_DIRECTORY_INDEX].page_table_base_address = page_dir_physical_addr >> 12;
        return page_dir;
    }
    return NULL;
}

bool paging_free_page_directory(struct PageDirectory *page_dir) {
    for (uint32_t i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++) {
        if (&page_directory_list[i] != page_dir) continue;
        if (!page_directory_manager.page_directory_used[i]) return false;

        // Page directory yang dihapus masih di CR3. Kalo dikosongkan langsung, mapping kernel ikut hilang saat kode kernel masih berjalan, sehingga hasilnya page fault.
        if (page_dir == paging_get_current_page_directory_addr())
            paging_use_page_directory(&_paging_kernel_page_directory);

        // Bebaskan frame page table user.
        for (uint32_t pde = 0; pde < 0x300; pde++) {
            if (!page_dir->table[pde].flag.present_bit) continue;
            uint32_t frame = page_dir->table[pde].page_table_base_address;
            page_manager_state.page_frame_map[frame] = false;
            page_manager_state.free_page_frame_count++;
        }

        memset(page_dir, 0, sizeof(struct PageDirectory));
        page_directory_manager.page_directory_used[i] = false;
        return true;
    }
    return false;
}

void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr) {
    uint32_t physical_addr_page_dir = (uint32_t) page_dir_virtual_addr;
    // Additional layer of check & mistake safety net
    if ((uint32_t) page_dir_virtual_addr >= KERNEL_VIRTUAL_ADDRESS_BASE)
        physical_addr_page_dir -= KERNEL_VIRTUAL_ADDRESS_BASE;
    __asm__  volatile("mov %0, %%cr3" : /* <Empty> */ : "r"(physical_addr_page_dir): "memory");
}
