#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/ext2.h"

static int row = 0;

static void print(const char *s, uint8_t fg) {
    int col = 0;
    while (*s && col < FRAMEBUFFER_WIDTH)
        framebuffer_write(row, col++, *s++, fg, 0);
    row++;
}

/* cetak "label : kode" untuk hasil operasi file system */
static void print_result(const char *label, int8_t code) {
    int col = 0;
    while (*label && col < FRAMEBUFFER_WIDTH)
        framebuffer_write(row, col++, *label++, 0xF, 0);
    framebuffer_write(row, col++, ' ', 0xF, 0);
    if (code < 0) {
        framebuffer_write(row, col++, '-', 0xE, 0);
        code = -code;
    }
    framebuffer_write(row, col, '0' + code, code == 0 ? 0xA : 0xE, 0);
    row++;
}

/* inode anak bernama name di dalam folder root, 0 kalau tidak ada */
static uint32_t find_in_root(char *name, uint8_t name_len) {
    uint8_t table[DIR_MAX_BLOCKS * BLOCK_SIZE];
    struct EXT2DriverRequest req = {table, ".", 1, 1, sizeof(table), true};
    if (read_directory(&req) != 0)
        return 0;
    int32_t offset = dir_find(table, req.buffer_size, name, name_len);
    if (offset < 0)
        return 0;
    return get_directory_entry(table, offset)->inode;
}

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);

    initialize_filesystem_ext2();
    print("== Demo File System OSjur (Chapter 2) ==", 0xB);
    print("Boot pertama: write = 0. Boot kedua: write = 1 (sudah ada, bukti persisten)", 0x7);
    row++;

    // 1. buat folder "dokumen" di root
    struct EXT2DriverRequest req = {0, "dokumen", 7, 1, 0, true};
    print_result("write folder /dokumen          :", write(&req));

    // 2. tulis file "halo.txt" di dalam /dokumen
    uint32_t dokumen = find_in_root("dokumen", 7);
    char isi[] = "Halo dari OSjur! File ini tetap ada setelah reboot.";
    struct EXT2DriverRequest file = {isi, "halo.txt", 8, dokumen, sizeof(isi), false};
    print_result("write file /dokumen/halo.txt   :", write(&file));

    // 3. baca lagi isinya
    char buf[128];
    for (int i = 0; i < 128; i++) buf[i] = 0;
    struct EXT2DriverRequest baca = {buf, "halo.txt", 8, dokumen, sizeof(buf), false};
    print_result("read /dokumen/halo.txt         :", read(baca));
    print(buf, 0xA);

    // 4. file sementara: tulis lalu hapus
    char tmp[] = "sementara";
    struct EXT2DriverRequest sementara = {tmp, "tmp.txt", 7, 1, sizeof(tmp), false};
    print_result("write file /tmp.txt            :", write(&sementara));
    print_result("delete /tmp.txt                :", delete(sementara));
    print_result("read /tmp.txt (harus 3)        :", read(sementara));

    // 5. folder yang masih berisi tidak boleh dihapus
    print_result("delete /dokumen (harus 2)      :", delete(req));

    // 6. isi root
    row++;
    print("Isi root:", 0xB);
    uint8_t table[DIR_MAX_BLOCKS * BLOCK_SIZE];
    struct EXT2DriverRequest root = {table, ".", 1, 1, sizeof(table), true};
    if (read_directory(&root) == 0) {
        uint32_t offset = 0;
        while (offset < root.buffer_size) {
            struct EXT2DirectoryEntry *entry = get_directory_entry(table, offset);
            if (entry->rec_len == 0)
                break; // root kecil, cukup satu block
            if (entry->inode != 0) { // inode 0 = slot kosong
                char name[64];
                uint8_t len = entry->name_len < 60 ? entry->name_len : 60;
                for (int i = 0; i < len; i++) name[i] = get_entry_name(entry)[i];
                name[len] = (entry->file_type == EXT2_FT_DIR) ? '/' : 0;
                name[len + 1] = 0;
                print(name, entry->file_type == EXT2_FT_DIR ? 0x9 : 0xF);
            }
            offset += entry->rec_len;
        }
    }

    while (true);
}