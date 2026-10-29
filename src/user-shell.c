#include <stdint.h>
#include <stdbool.h>
#include "header/filesystem/ext2.h"

/**
 * Panggil system call kernel lewat int 0x30.
 * Register diisi langsung lewat constraint "a", "b", "c", "d" supaya gcc tidak
 * memakai salah satu register itu sebagai register perantara di antara instruksi mov.
 */
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("int $0x30" : /* <Empty> */ : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

int main(void) {
    struct BlockBuffer       bl[2]   = {0};
    struct EXT2DriverRequest request = {
        .buf          = &bl,
        .name         = "shell",
        .name_len     = 5,
        .parent_inode = 1,
        .buffer_size  = sizeof(bl),
    };
    int8_t retcode = -1;
    syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);
    if (retcode == 0)
        syscall(6, (uint32_t) "owo\n", 4, 0xF);

    char buf;
    syscall(7, 0, 0, 0);
    while (true) {
        syscall(4, (uint32_t) &buf, 0, 0);
        if (buf != 0)
            syscall(5, (uint32_t) buf, 0xF, 0);
    }
    return 0;
}