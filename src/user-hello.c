#include <stdint.h>

/* Program uji paling sederhana untuk Ch.4: cetak satu baris lalu keluar.
 * Keluar dari main sekaligus menguji syscall exit di crt0.s. */

#define SYSCALL_PUTS 6
#define COLOR_TEXT   0xF

static void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("int $0x30" : : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

int main(void) {
    static const char msg[] = "Hello World!\n";
    syscall(SYSCALL_PUTS, (uint32_t) msg, sizeof(msg) - 1, COLOR_TEXT);
    return 0;
}
