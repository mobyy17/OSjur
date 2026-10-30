#include "header/shell/shell.h"

/* cat <file>...: tulis isi file di folder sekarang sebagai teks.
 * Tanpa argumen, cat meneruskan input dari pipe (contoh: ls | cat). */
void cmd_cat(int argc, char *argv[]) {
    if (argc == 1) {
        if (pipe_in != 0)
            out_write(pipe_in, pipe_in_len);
        else
            out_puts("pemakaian: cat <file>...\n");
        return;
    }

    char buf[SHELL_FILE_MAX];
    for (int i = 1; i < argc; i++) {
        uint32_t size = 0;
        int8_t retcode = file_read(cwd_inode, argv[i], buf, &size);

        if (retcode == 0) {
            out_write(buf, size);
            // supaya prompt berikutnya tidak menempel di baris terakhir file
            if (size > 0 && buf[size - 1] != '\n')
                out_putchar('\n');
            continue;
        }

        out_puts("cat: ");
        out_puts(argv[i]);
        if (retcode == 1)
            out_puts(" adalah folder\n");
        else if (retcode == 2)
            out_puts(" terlalu besar (maks 64 KiB)\n");
        else if (retcode == 3)
            out_puts(" tidak ditemukan\n");
        else
            out_puts(" gagal dibaca\n");
    }
}
