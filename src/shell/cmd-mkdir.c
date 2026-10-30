#include "header/shell/shell.h"

/* mkdir <nama>: buat folder kosong baru di folder sekarang. */
void cmd_mkdir(int argc, char *argv[]) {
    if (argc != 2) {
        out_puts("pemakaian: mkdir <nama>\n");
        return;
    }

    struct EXT2DriverRequest request = {
        .buf          = 0,
        .name         = argv[1],
        .name_len     = str_len(argv[1]),
        .parent_inode = cwd_inode,
        .buffer_size  = 0,
        .is_directory = true,
    };
    int8_t retcode = -1;
    syscall(SYSCALL_WRITE, (uint32_t) &request, (uint32_t) &retcode, 0);

    if (retcode == 0)
        return;
    out_puts("mkdir: ");
    out_puts(argv[1]);
    if (retcode == 1)
        out_puts(" sudah ada\n");
    else
        out_puts(" gagal dibuat (disk atau folder penuh?)\n");
}