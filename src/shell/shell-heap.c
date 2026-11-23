#include "header/shell/shell.h"

/* malloc/free versi user: cuma meneruskan ke kernel lewat syscall 22 dan 23. Alokasinya sendiri (first-fit, split, coalescing) ada di kernel, src/memory/heap.c. */

void *malloc(uint32_t size) {
    uint32_t addr = 0;
    syscall(SYSCALL_MALLOC, size, (uint32_t) &addr, 0);
    return (void*) addr;
}

void free(void *ptr) {
    if (ptr == 0)
        return;
    bool ok = false;
    syscall(SYSCALL_FREE, (uint32_t) ptr, (uint32_t) &ok, 0);
    if (!ok)
        print("free: pointer tidak valid atau sudah di-free\n", COLOR_TEXT);
}

static void out_hex(uint32_t v) {
    char s[11] = "0x00000000";
    for (int i = 9; i >= 2; i--, v >>= 4)
        s[i] = "0123456789ABCDEF"[v & 0xF];
    out_puts(s);
}

void out_dec(uint32_t v) {
    char s[11];
    int i = 10;
    s[i] = '\0';
    do {
        s[--i] = (char) ('0' + v % 10);
        v /= 10;
    } while (v > 0);
    out_puts(&s[i]);
}

/* meminfo [-v]: ringkasan heap shell, -v menampilkan daftar semua blok */
void cmd_meminfo(int argc, char *argv[]) {
    struct HeapInfo info;
    syscall(SYSCALL_HEAP_INFO, (uint32_t) &info, 0, 0);
    out_puts("heap shell 0x00100000-0x00380000, total ");
    out_dec(info.total);
    out_puts(" byte\n  terpakai ");
    out_dec(info.used);
    out_puts(" byte (");
    out_dec(info.used_blocks);
    out_puts(" blok), kosong ");
    out_dec(info.free);
    out_puts(" byte, blok kosong terbesar ");
    out_dec(info.largest_free);
    out_puts("\n");

    if (argc < 2 || !str_eq(argv[1], "-v"))
        return;
    for (uint32_t i = 0;; i++) {
        struct HeapBlockInfo block;
        bool found = false;
        syscall(SYSCALL_HEAP_BLOCK, i, (uint32_t) &block, (uint32_t) &found);
        if (!found)
            break;
        out_puts("  [");
        out_dec(i);
        out_puts("] ");
        out_hex(block.addr);
        out_puts(block.used ? " dipakai " : " kosong  ");
        out_dec(block.size);
        out_puts(" byte\n");
    }
}

void out_hex32(uint32_t v) {
    out_hex(v);
}
