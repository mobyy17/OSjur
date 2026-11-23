#include "header/shell/shell.h"

/* bg [warna]: ganti warna background shell. Tanpa argumen menampilkan pilihan. */

static const char *bg_names[] = {
    "hitam", "biru", "lumut", "cyan", "merah", "ungu", "coklat", "abu",
};
static const uint8_t bg_colors[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x8};
#define BG_COUNT (sizeof(bg_colors) / sizeof(bg_colors[0]))

void cmd_bg(int argc, char *argv[]) {
    if (argc == 2) {
        for (uint32_t i = 0; i < BG_COUNT; i++) {
            if (str_eq(argv[1], bg_names[i])) {
                syscall(SYSCALL_SET_BG, bg_colors[i], 0, 0);
                return;
            }
        }
    }
    out_puts("pemakaian: bg <warna>, pilihan:");
    for (uint32_t i = 0; i < BG_COUNT; i++) {
        out_puts(" ");
        out_puts(bg_names[i]);
    }
    out_puts("\n");
}
