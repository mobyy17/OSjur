#include <stdint.h>
#include <stdbool.h>

/* echo: cetak semua argumen dipisah spasi. Bukti argc/argv sampai ke program user. */

#define SYSCALL_PUTS 6
#define COLOR_TEXT   0xF

static uint32_t str_len(const char *s) {
    uint32_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

static void write_str(const char *s, uint32_t len) {
    __asm__ volatile("int $0x30" : : "a"(SYSCALL_PUTS), "b"(s), "c"(len), "d"(COLOR_TEXT) : "memory");
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        write_str(argv[i], str_len(argv[i]));
        if (i + 1 < argc)
            write_str(" ", 1);
    }
    write_str("\n", 1);
    return 0;
}
