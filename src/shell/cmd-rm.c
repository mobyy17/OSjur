#include "header/shell/shell.h"

/* rm <file>: hapus file di folder sekarang. Hapus folder (bonus) belum didukung. */
void cmd_rm(int argc, char *argv[]) {
    if (argc != 2) {
        out_puts("pemakaian: rm <file>\n");
        return;
    }

    uint32_t inode;
    bool is_dir;
    if (!dir_lookup(cwd_inode, argv[1], &inode, &is_dir)) {
        out_puts("rm: ");
        out_puts(argv[1]);
        out_puts(" tidak ditemukan\n");
        return;
    }
    if (is_dir) {
        out_puts("rm: ");
        out_puts(argv[1]);
        out_puts(" adalah folder, hapus folder belum didukung\n");
        return;
    }

    struct EXT2DriverRequest request = {
        .buf          = 0,
        .name         = argv[1],
        .name_len     = str_len(argv[1]),
        .parent_inode = cwd_inode,
        .buffer_size  = 0,
        .is_directory = false,
    };
    int8_t retcode = -1;
    syscall(SYSCALL_DELETE, (uint32_t) &request, (uint32_t) &retcode, 0);

    if (retcode != 0) {
        out_puts("rm: gagal menghapus ");
        out_puts(argv[1]);
        out_putchar('\n');
    }
}
