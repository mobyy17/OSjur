#include "header/shell/shell.h"

/* Batas sh di dalam sh, supaya script yang memanggil dirinya sendiri tidak berputar terus */
#define SH_DEPTH_MAX 3

static uint32_t sh_depth = 0;

/* sh <file.sh>: jalankan isi file baris per baris, sama seperti diketik di prompt.
 * Baris kosong dan baris yang diawali '#' dilewati. */
void cmd_sh(int argc, char *argv[]) {
    if (argc != 2) {
        out_puts("pemakaian: sh <file.sh>\n");
        return;
    }
    if (sh_depth >= SH_DEPTH_MAX) {
        out_puts("sh: script bersarang terlalu dalam\n");
        return;
    }

    char script[SHELL_FILE_MAX];
    uint32_t size = 0;
    int8_t retcode = file_read(cwd_inode, argv[1], script, &size);
    if (retcode != 0) {
        out_puts("sh: ");
        out_puts(argv[1]);
        out_puts(retcode == 1 ? " adalah folder\n" : " tidak bisa dibaca\n");
        return;
    }

    sh_depth++;
    uint32_t i = 0;
    while (i < size) {
        // Salin satu baris ke buffer sendiri, karena shell_run_line mengubah isinya
        char line[SHELL_LINE_MAX];
        uint32_t len = 0;
        while (i < size && script[i] != '\n') {
            if (script[i] != '\r' && len < SHELL_LINE_MAX - 1)
                line[len++] = script[i];
            i++;
        }
        i++; // lewati '\n'
        line[len] = '\0';

        uint32_t start = 0;
        while (line[start] == ' ')
            start++;
        if (line[start] == '\0' || line[start] == '#')
            continue;

        out_flush();
        shell_run_line(&line[start]);
        out_flush();
    }
    sh_depth--;
}
