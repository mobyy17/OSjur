#include "header/shell/shell.h"

/* cp <file> <tujuan>
 * - tujuan adalah folder di folder sekarang: file disalin ke dalamnya dengan nama yang sama
 * - selain itu: file disalin di folder sekarang dengan nama tujuan
 * Tujuan yang sudah ada tidak ditimpa. Copy folder (bonus) belum didukung. */
void cmd_cp(int argc, char *argv[]) {
    if (argc != 3) {
        out_puts("pemakaian: cp <file> <tujuan>\n");
        return;
    }

    char buf[SHELL_FILE_MAX];
    uint32_t size = 0;
    int8_t retcode = file_read(cwd_inode, argv[1], buf, &size);
    if (retcode != 0) {
        out_puts("cp: ");
        out_puts(argv[1]);
        if (retcode == 1)
            out_puts(" adalah folder, copy folder belum didukung\n");
        else if (retcode == 2)
            out_puts(" terlalu besar (maks 64 KiB)\n");
        else if (retcode == 3)
            out_puts(" tidak ditemukan\n");
        else
            out_puts(" gagal dibaca\n");
        return;
    }

    uint32_t parent = cwd_inode;
    char *name      = argv[2];
    uint32_t inode;
    bool is_dir;
    if (dir_lookup(cwd_inode, argv[2], &inode, &is_dir) && is_dir) {
        parent = inode;
        name   = argv[1];
    }

    struct EXT2DriverRequest request = {
        .buf          = buf,
        .name         = name,
        .name_len     = str_len(name),
        .parent_inode = parent,
        .buffer_size  = size,
        .is_directory = false,
    };
    retcode = -1;
    syscall(SYSCALL_WRITE, (uint32_t) &request, (uint32_t) &retcode, 0);

    if (retcode == 1) {
        out_puts("cp: ");
        out_puts(name);
        out_puts(" sudah ada\n");
    } else if (retcode != 0) {
        out_puts("cp: gagal menulis ");
        out_puts(name);
        out_puts(" (disk penuh?)\n");
    }
}
