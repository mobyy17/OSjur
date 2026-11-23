#include <stdint.h>
#include <stdbool.h>

/* args: tampilkan argc dan isi argv[i] satu per satu (untuk demo bonus C runtime) */

#define SYSCALL_PUTS 6

static uint32_t str_len(const char *s) {
    uint32_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

static void print(const char *s, uint32_t color) {
    __asm__ volatile("int $0x30" : : "a"(SYSCALL_PUTS), "b"(s), "c"(str_len(s)), "d"(color) : "memory");
}

int main(int argc, char *argv[]) {
    char num[4] = "0";
    num[0] = '0' + argc % 10;
    print("argc = ", 0x7);
    print(num, 0xE);
    print("\n", 0x7);
    for (int i = 0; i < argc; i++) {
        num[0] = '0' + i;
        print("argv[", 0x7);
        print(num, 0x7);
        print("] = \"", 0x7);
        print(argv[i], 0xA);
        print("\"\n", 0x7);
    }
    return argc;
}
