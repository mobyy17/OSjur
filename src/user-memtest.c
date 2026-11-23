#include <stdint.h>
#include <stdbool.h>

/* memtest: uji malloc/free di heap process ini sendiri.
 * 1. malloc 100 blok x 1 KiB, isi tiap blok dengan pola, free yang index genap
 * 2. malloc 1 KiB lagi, harus memakai lubang bekas blok genap (alamat lama)
 * 3. free sisanya, heap harus kembali jadi satu blok kosong (coalescing)
 * 4. malloc melebihi heap harus gagal (0), bukan page fault
 * 5. free dua kali pada pointer yang sama harus ditolak */

#define SYSCALL_PUTS      6
#define SYSCALL_MALLOC    22
#define SYSCALL_FREE      23
#define SYSCALL_HEAP_INFO 24

struct HeapInfo {
    uint32_t total, used, free, block_count, used_blocks, largest_free;
};

static void sys(uint32_t eax, uint32_t ebx, uint32_t ecx) {
    __asm__ volatile("int $0x30" : : "a"(eax), "b"(ebx), "c"(ecx), "d"(0) : "memory");
}

static void *malloc(uint32_t size) {
    uint32_t addr = 0;
    sys(SYSCALL_MALLOC, size, (uint32_t) &addr);
    return (void*) addr;
}

static bool free(void *ptr) {
    bool ok = false;
    sys(SYSCALL_FREE, (uint32_t) ptr, (uint32_t) &ok);
    return ok;
}

static void puts(const char *s) {
    uint32_t len = 0;
    while (s[len] != '\0')
        len++;
    __asm__ volatile("int $0x30" : : "a"(SYSCALL_PUTS), "b"(s), "c"(len), "d"(0xF) : "memory");
}

static void put_hex(uint32_t v) {
    char s[11] = "0x00000000";
    for (int i = 9; i >= 2; i--, v >>= 4)
        s[i] = "0123456789ABCDEF"[v & 0xF];
    puts(s);
}

static void put_dec(uint32_t v) {
    char s[11];
    int i = 10;
    s[i] = '\0';
    do {
        s[--i] = (char) ('0' + v % 10);
        v /= 10;
    } while (v > 0);
    puts(&s[i]);
}

static void check(const char *name, bool ok) {
    puts(ok ? "  [OK]   " : "  [GAGAL] ");
    puts(name);
    puts("\n");
}

int main(void) {
    static uint8_t *blocks[100];
    bool pattern_ok = true;

    for (uint32_t i = 0; i < 100; i++) {
        blocks[i] = malloc(1024);
        if (blocks[i] == 0) {
            pattern_ok = false;
            break;
        }
        for (uint32_t j = 0; j < 1024; j++)
            blocks[i][j] = (uint8_t) (i + j);
    }
    for (uint32_t i = 0; i < 100 && pattern_ok; i++)
        for (uint32_t j = 0; j < 1024; j++)
            if (blocks[i][j] != (uint8_t) (i + j))
                pattern_ok = false;
    puts("malloc 100 x 1 KiB, blok pertama di ");
    put_hex((uint32_t) blocks[0]);
    puts("\n");
    check("isi tiap blok tidak saling menimpa", pattern_ok);

    for (uint32_t i = 0; i < 100; i += 2)
        free(blocks[i]);
    uint8_t *reuse = malloc(1000);
    check("malloc baru memakai lubang bekas blok 0", reuse == blocks[0]);

    struct HeapInfo info;
    sys(SYSCALL_HEAP_INFO, (uint32_t) &info, 0);
    puts("  setelah free yang genap: ");
    put_dec(info.block_count);
    puts(" blok, ");
    put_dec(info.used_blocks);
    puts(" dipakai\n");

    free(reuse);
    for (uint32_t i = 1; i < 100; i += 2)
        free(blocks[i]);
    sys(SYSCALL_HEAP_INFO, (uint32_t) &info, 0);
    check("semua di-free, heap kembali jadi 1 blok kosong", info.block_count == 1 && info.used == 0);

    check("malloc 4 MiB ditolak (return 0)", malloc(4 * 1024 * 1024) == 0);
    check("free dua kali ditolak", !free(blocks[1]));
    check("free pointer ngawur ditolak", !free((void*) 0x200004));
    return 0;
}
