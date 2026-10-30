#include "header/shell/shell.h"

/* mv <sumber> <tujuan>
 * - tujuan adalah folder yang sudah ada di folder sekarang: sumber dipindah ke dalamnya, nama tetap
 *   ("mv a.txt folder1", "mv .. " juga boleh sebagai tujuan untuk naik satu folder)
 * - selain itu: sumber di-rename menjadi tujuan di folder yang sama ("mv a.txt b.txt")
 * Bisa untuk file maupun folder. Pemindahan dikerjakan kernel lewat syscall move (8),
 * cukup mengubah directory entry, isi file tidak disalin. */
void cmd_mv(int argc, char *argv[]) {
    if (argc != 3) {
        out_puts("pemakaian: mv <sumber> <tujuan>\n");
        return;
    }

    struct EXT2DriverRequest src = {
        .buf          = 0,
        .name         = argv[1],
        .name_len     = str_len(argv[1]),
        .parent_inode = cwd_inode,
        .buffer_size  = 0,
        .is_directory = false,
    };
    struct EXT2DriverRequest dst = {
        .buf          = 0,
        .name         = argv[2],
        .name_len     = str_len(argv[2]),
        .parent_inode = cwd_inode,
        .buffer_size  = 0,
        .is_directory = false,
    };

    uint32_t inode;
    bool is_dir;
    if (dir_lookup(cwd_inode, argv[2], &inode, &is_dir) && is_dir) {
        dst.parent_inode = inode; // pindah ke dalam folder, nama tetap
        dst.name         = argv[1];
        dst.name_len     = src.name_len;
    }

    int8_t retcode = -1;
    syscall(SYSCALL_MOVE, (uint32_t) &src, (uint32_t) &retcode, (uint32_t) &dst);

    if (retcode == 0)
        return;
    out_puts("mv: ");
    if (retcode == 1) {
        out_puts(argv[1]);
        out_puts(" tidak ditemukan\n");
    } else if (retcode == 2) {
        out_puts(dst.name);
        out_puts(" sudah ada di tujuan\n");
    } else {
        out_puts("gagal memindahkan ");
        out_puts(argv[1]);
        out_putchar('\n');
    }
}
