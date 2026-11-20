#include "header/shell/shell.h"

/* Ukuran maksimum executable, sama dengan batas 1 MiB di user-linker.ld */
#define EXEC_BUFFER_SIZE (1024 * 1024)

/* exec <file>: jalankan file di folder sekarang sebagai process baru.
 * Shell tidak menunggu process itu selesai. */
void cmd_exec(int argc, char *argv[]) {
    if (argc != 2) {
        out_puts("pemakaian: exec <file>\n");
        return;
    }

    // Shell kedua bakal rebutan input keyboard sama shell yang sekarang
    if (str_eq(argv[1], "shell")) {
        out_puts("exec: shell sudah berjalan\n");
        return;
    }

    // Cek dulu di sisi shell supaya pesan error lebih jelas dari kode FS_READ_FAILURE
    uint32_t inode = 0;
    bool     is_dir = false;
    if (!dir_lookup(cwd_inode, argv[1], &inode, &is_dir)) {
        out_puts("exec: ");
        out_puts(argv[1]);
        out_puts(" tidak ditemukan\n");
        return;
    }
    if (is_dir) {
        out_puts("exec: ");
        out_puts(argv[1]);
        out_puts(" adalah folder\n");
        return;
    }

    // Executable selalu dimuat di virtual address 0 milik process baru
    struct EXT2DriverRequest request = {
        .buf          = (void*) 0,
        .name         = argv[1],
        .name_len     = (uint8_t) str_len(argv[1]),
        .parent_inode = cwd_inode,
        .buffer_size  = EXEC_BUFFER_SIZE,
        .is_directory = false,
    };
    int32_t retcode = -1;
    syscall(SYSCALL_EXEC, (uint32_t) &request, (uint32_t) &retcode, 0);

    if (retcode == EXEC_SUCCESS)
        return;

    out_puts("exec: ");
    out_puts(argv[1]);
    if (retcode == EXEC_FAIL_MAX_PROCESS_EXCEEDED)
        out_puts(": jumlah process sudah maksimum\n");
    else if (retcode == EXEC_FAIL_INVALID_ENTRYPOINT)
        out_puts(": entrypoint tidak valid\n");
    else if (retcode == EXEC_FAIL_NOT_ENOUGH_MEMORY)
        out_puts(": memory tidak cukup\n");
    else if (retcode == EXEC_FAIL_FS_READ_FAILURE)
        out_puts(": gagal dibaca (file lebih dari 1 MiB?)\n");
    else
        out_puts(": gagal dijalankan\n");
}
