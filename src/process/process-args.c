#include "header/process/process-args.h"
#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"

#define USER_STACK_TOP (0x400000u - 4)

/* Cari PCB aktif dengan PID terbesar, yaitu process yang barusan dibuat */
static struct ProcessControlBlock *newest_process(void) {
    struct ProcessControlBlock *newest = NULL;
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        struct ProcessControlBlock *pcb = &_process_list[i];
        if (pcb->metadata.active && (newest == NULL || pcb->metadata.pid > newest->metadata.pid))
            newest = pcb;
    }
    return newest;
}

int32_t process_create_user_process_with_args(struct EXT2DriverRequest request, struct ExecArgs *args) {
    // 1. Salin argumen ke stack kernel dulu. Pointer argv milik shell tidak valid lagi begitu CR3 dipindah ke page directory process baru.
    char     arg_copy[SYSCALL_EXT_ARGC_MAX][SYSCALL_EXT_ARG_LEN];
    uint32_t arg_len[SYSCALL_EXT_ARGC_MAX];
    uint32_t argc = args->argc;
    if (argc > SYSCALL_EXT_ARGC_MAX)
        argc = SYSCALL_EXT_ARGC_MAX;

    for (uint32_t i = 0; i < argc; i++) {
        char *src = args->argv[i];
        uint32_t len = 0;
        while ((uint32_t) (src + len) < 0x400000 && src[len] != '\0' && len < SYSCALL_EXT_ARG_LEN - 1) {
            arg_copy[i][len] = src[len];
            len++;
        }
        arg_copy[i][len] = '\0';
        arg_len[i] = len + 1; // termasuk '\0'
    }

    // 2. Process dibuat seperti biasa
    int32_t retcode = process_create_user_process(request);
    if (retcode != PROCESS_CREATE_SUCCESS)
        return retcode;

    struct ProcessControlBlock *pcb = newest_process();
    args->pid = pcb->metadata.pid; // args masih bisa ditulis, CR3 masih milik pemanggil

    // 3. Tulis argumen ke stack process baru. Dari alamat tinggi ke rendah dimana string-string argumen, argv[argc] = 0, argv[argc-1] ... argv[0], pointer argv, argc. crt0 melakukan "call main", jadi di dalam main [esp+4] = argc dan [esp+8] = argv (cdecl).
    struct PageDirectory *prev = paging_get_current_page_directory_addr();
    paging_use_page_directory(pcb->context.page_directory_virtual_addr);

    uint32_t sp = USER_STACK_TOP;
    uint32_t user_argv[SYSCALL_EXT_ARGC_MAX + 1];
    for (int32_t i = (int32_t) argc - 1; i >= 0; i--) {
        sp -= arg_len[i];
        sp &= ~3u; // jaga alignment 4 byte
        memcpy((void*) sp, arg_copy[i], arg_len[i]);
        user_argv[i] = sp;
    }
    user_argv[argc] = 0;

    sp -= (argc + 1) * sizeof(uint32_t);
    memcpy((void*) sp, user_argv, (argc + 1) * sizeof(uint32_t));
    uint32_t argv_addr = sp;

    sp -= 4;
    *((uint32_t*) sp) = argv_addr;
    sp -= 4;
    *((uint32_t*) sp) = argc;

    paging_use_page_directory(prev);
    pcb->context.cpu.stack.esp = sp;
    return PROCESS_CREATE_SUCCESS;
}
