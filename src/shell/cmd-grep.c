#include "header/shell/shell.h"

/* grep <pola> [file]: tulis setiap baris yang mengandung <pola>, satu baris per hasil.
 * - grep <pola> <file> : input dari file di folder sekarang
 * - ... | grep <pola>  : input dari output command sebelumnya (contoh: ls | grep txt)
 * Pola dicocokkan apa adanya (substring), regex belum didukung. */

/* true kalau pattern muncul di dalam line (line tidak null-terminated) */
static bool line_contains(const char *line, uint32_t line_len, const char *pattern, uint32_t pattern_len) {
    if (pattern_len > line_len) return false;
    for (uint32_t start = 0; start + pattern_len <= line_len; start++) {
        uint32_t i = 0;
        while (i < pattern_len && line[start + i] == pattern[i]) i++;
        if (i == pattern_len) return true;
    }
    return false;
}

/* Potong text per '\n', tulis baris yang cocok. Baris terakhir tanpa '\n' tetap diperiksa. */
static void grep_text(const char *text, uint32_t len, const char *pattern) {
    uint32_t pattern_len = str_len(pattern);
    uint32_t start = 0;
    for (uint32_t i = 0; i <= len; i++) {
        if (i == len || text[i] == '\n') {
            uint32_t line_len = i - start;
            if (line_len > 0 && line_contains(text + start, line_len, pattern, pattern_len)) {
                out_write(text + start, line_len);
                out_putchar('\n');
            }
            start = i + 1;
        }
    }
}

void cmd_grep(int argc, char *argv[]) {
    // dari pipe: "ls | grep txt"
    if (argc == 2 && pipe_in != 0) {
        grep_text(pipe_in, pipe_in_len, argv[1]);
        return;
    }

    if (argc != 3) {
        out_puts("pemakaian: grep <pola> <file>  atau  ... | grep <pola>\n");
        return;
    }

    // dari file: "grep halo note.txt"
    char buf[SHELL_FILE_MAX];
    uint32_t size = 0;
    int8_t retcode = file_read(cwd_inode, argv[2], buf, &size);
    if (retcode == 0) {
        grep_text(buf, size, argv[1]);
        return;
    }

    out_puts("grep: ");
    out_puts(argv[2]);
    if (retcode == 1)
        out_puts(" adalah folder\n");
    else if (retcode == 2)
        out_puts(" terlalu besar (maks 64 KiB)\n");
    else if (retcode == 3)
        out_puts(" tidak ditemukan\n");
    else
        out_puts(" gagal dibaca\n");
}