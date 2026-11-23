#include "header/shell/shell.h"

/* Ganti isi baris input yang tampil di layar dengan new_line */
static uint32_t replace_line(char *line, uint32_t len, const char *new_line, uint32_t max_len) {
    for (uint32_t i = 0; i < len; i++)
        syscall(SYSCALL_PUTCHAR, '\b', COLOR_TEXT, 0);

    uint32_t new_len = 0;
    while (new_line[new_len] != '\0' && new_len < max_len) {
        line[new_len] = new_line[new_len];
        syscall(SYSCALL_PUTCHAR, (uint32_t) line[new_len], COLOR_TEXT, 0);
        new_len++;
    }
    return new_len;
}

/**
 * Sama seperti read_line versi Ch.3, ditambah:
 * - panah atas/bawah untuk memanggil perintah sebelumnya
 * - Tab untuk autocomplete nama command atau file
 * Input tetap dibatasi max_len supaya baris tidak terlipat ke bawah.
 */
bool shell_read_line(char *line, uint32_t max_len) {
    uint32_t len = 0;
    history_reset_cursor();

    while (true) {
        char c = 0;
        syscall(SYSCALL_GETCHAR, (uint32_t) &c, 0, 0);
        if (c == 0)
            continue;

        // PageUp/PageDown menggulung output lama. Tombol lain mengembalikan tampilan ke bawah dulu.
        if (c == KEY_PGUP_CHAR || c == KEY_PGDN_CHAR) {
            syscall(SYSCALL_SCROLL, (uint32_t) (c == KEY_PGUP_CHAR ? 12 : -12), 0, 0);
            continue;
        }
        syscall(SYSCALL_SCROLL, 0, 0, 0);

        if (c == '\n') {
            syscall(SYSCALL_PUTCHAR, '\n', COLOR_TEXT, 0);
            line[len] = '\0';
            history_push(line);
            return true;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                syscall(SYSCALL_PUTCHAR, '\b', COLOR_TEXT, 0);
            }
        } else if (c == KEY_CTRL_C_CHAR) {
            print("^C\n", COLOR_TEXT);
            return false;
        } else if (c == KEY_UP_CHAR) {
            const char *prev = history_prev();
            if (prev != 0)
                len = replace_line(line, len, prev, max_len);
        } else if (c == KEY_DOWN_CHAR) {
            const char *next = history_next();
            len = replace_line(line, len, next != 0 ? next : "", max_len);
        } else if (c == '\t') {
            line[len] = '\0';
            uint32_t new_len = autocomplete(line, len, max_len);
            // autocomplete mungkin mencetak daftar kandidat lalu prompt baru, jadi tulis ulang semua yang belum tampil
            for (uint32_t i = len; i < new_len; i++)
                syscall(SYSCALL_PUTCHAR, (uint32_t) line[i], COLOR_TEXT, 0);
            len = new_len;
        } else if (c >= ' ' && len < max_len) {
            line[len++] = c;
            syscall(SYSCALL_PUTCHAR, (uint32_t) c, COLOR_TEXT, 0);
        }
    }
}
