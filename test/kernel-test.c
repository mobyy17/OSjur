#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"

#define ROOT_INODE 1
#define LEN(s) ((uint8_t) (sizeof(s) - 1))

static int slot = 0;
static int pass_count = 0;
static int fail_count = 0;

static void print_at(int row, int col, const char *s, uint8_t fg) {
    while (*s && col < FRAMEBUFFER_WIDTH) framebuffer_write(row, col++, *s++, fg, 0);
}

static void report(const char *label, bool ok, int8_t got) {
    int row = 2 + slot % 21;
    int col = (slot / 21) * 40;
    slot++;

    print_at(row, col, label, 0x7);
    if (ok) {
        print_at(row, col + 32, "PASS", 0xA);
        pass_count++;
    } else {
        print_at(row, col + 32, "FAIL", 0xC);
        // tampilkan error code yang didapat supaya mudah dilacak
        char code[4] = {' ', ' ', ' ', 0};
        if (got < 0) {
            code[0] = '-';
            got = -got;
        }
        code[1] = '0' + got;
        print_at(row, col + 36, code, 0xC);
        fail_count++;
    }
}

static void expect(const char *label, int8_t got, int8_t want) {
    report(label, got == want, got);
}


static struct EXT2DriverRequest req(void *buf, char *name, uint8_t name_len, uint32_t parent, uint32_t size, bool is_dir) {
    struct EXT2DriverRequest r = {
        .buf          = buf,
        .name         = name,
        .name_len     = name_len,
        .parent_inode = parent,
        .buffer_size  = size,
        .is_directory = is_dir,
    };
    return r;
}

static uint8_t dir_buf[DIR_MAX_BLOCKS * BLOCK_SIZE];
static int8_t read_dir(char *name, uint8_t name_len, uint32_t parent) {
    struct EXT2DriverRequest r = req(dir_buf, name, name_len, parent, sizeof(dir_buf), true);
    return read_directory(&r);
}

static uint32_t inode_of(char *name, uint8_t name_len, uint32_t parent) {
    struct EXT2DriverRequest r = req(dir_buf, ".", 1, parent, sizeof(dir_buf), true);
    if (read_directory(&r) != 0) return 0;
    int32_t offset = dir_find(dir_buf, r.buffer_size, name, name_len);
    if (offset < 0) return 0;
    return get_directory_entry(dir_buf, offset)->inode;
}

/* apakah folder (sudah dibaca ke dir_buf, panjang size) punya entry "name" */
static bool dir_has(uint32_t size, char *name, uint8_t name_len) {
    return dir_find(dir_buf, size, name, name_len) >= 0;
}

/* buat baca sample-image.bin */
static void test_sample_image(void) {
    char buf[64];
    struct EXT2DriverRequest r;

    // file di root
    memset(buf, 0, sizeof(buf));
    r = req(buf, "laur", LEN("laur"), ROOT_INODE, sizeof(buf), false);
    int8_t code = read(r);
    report("A1 read /laur", code == 0 && memcmp(buf, "Grievous Lady", 13) == 0, code);

    memset(buf, 0, sizeof(buf));
    r = req(buf, "sasakure", LEN("sasakure"), ROOT_INODE, sizeof(buf), false);
    code = read(r);
    report("A2 read /sasakure", code == 0 && memcmp(buf, "TONDEMO-WONDERZ", 15) == 0, code);

    // file di dalam subfolder
    uint32_t folder1 = inode_of("folder1", LEN("folder1"), ROOT_INODE);
    memset(buf, 0, sizeof(buf));
    r = req(buf, "tpazolite", LEN("tpazolite"), folder1, sizeof(buf), false);
    code = read(r);
    report("A3 read /folder1/tpazolite", code == 0 && memcmp(buf, "Oshama Scramble!", 16) == 0, code);

    // read_directory folder1, isinya harus nestedf1, tpazolite, xi
    struct EXT2DriverRequest d = req(dir_buf, "folder1", LEN("folder1"), ROOT_INODE, sizeof(dir_buf), true);
    code = read_directory(&d);
    report("A4 read_dir /folder1", code == 0 && dir_has(d.buffer_size, "nestedf1", 8) && dir_has(d.buffer_size, "tpazolite", 9) && dir_has(d.buffer_size, "xi", 2), code);

    // folder bersarang dan folder kosong
    expect("A5 read_dir /folder1/nestedf1", read_dir("nestedf1", 8, folder1), 0);
    expect("A6 read_dir /folder2 (kosong)", read_dir("folder2", 7, ROOT_INODE), 0);

    // tipe salah
    r = req(buf, "folder1", LEN("folder1"), ROOT_INODE, sizeof(buf), false);
    expect("A7 read folder /folder1 (1)", read(r), 1);
    expect("A8 read_dir file /laur (1)", read_dir("laur", 4, ROOT_INODE), 1);
}

/* write, read, read_directory, delete */
static char isi_a[] = "Halo dari OSjur, ini isi a.txt";
static uint8_t big[150 * BLOCK_SIZE]; // wajib lewat doubly indirect
static uint8_t big_back[150 * BLOCK_SIZE];

static void test_crud(void) {
    struct EXT2DriverRequest r;
    char buf[64];

    // write folder
    r = req(0, "uji", LEN("uji"), ROOT_INODE, 0, true);
    expect("B1 write folder /uji", write(&r), 0);
    expect("B2 write /uji lagi (1)", write(&r), 1);
    uint32_t uji = inode_of("uji", LEN("uji"), ROOT_INODE);

    r = req(0, "sub", LEN("sub"), uji, 0, true);
    expect("B3 write folder /uji/sub", write(&r), 0);
    uint32_t sub = inode_of("sub", LEN("sub"), uji);

    // write file 
    r = req(isi_a, "a.txt", LEN("a.txt"), uji, sizeof(isi_a), false);
    expect("B4 write file /uji/a.txt", write(&r), 0);
    expect("B5 write a.txt lagi (1)", write(&r), 1);

    // nama sama di folder berbeda harus boleh
    r = req(isi_a, "a.txt", LEN("a.txt"), sub, sizeof(isi_a), false);
    expect("B6 write /uji/sub/a.txt", write(&r), 0);

    // parent bukan folder
    uint32_t file_a = inode_of("a.txt", LEN("a.txt"), uji);
    r = req(isi_a, "x.txt", LEN("x.txt"), file_a, sizeof(isi_a), false);
    expect("B7 write parent=file (2)", write(&r), 2);

    // read file 
    memset(buf, 0, sizeof(buf));
    r = req(buf, "a.txt", LEN("a.txt"), uji, sizeof(buf), false);
    int8_t code = read(r);
    report("B8 read /uji/a.txt + isi", code == 0 && memcmp(buf, isi_a, sizeof(isi_a)) == 0, code);

    r = req(buf, "a.txt", LEN("a.txt"), uji, 4, false);
    expect("B9 read buffer kecil (2)", read(r), 2);

    r = req(buf, "tidakada", LEN("tidakada"), uji, sizeof(buf), false);
    expect("B10 read tidak ada (3)", read(r), 3);

    r = req(buf, "a.txt", LEN("a.txt"), file_a, sizeof(buf), false);
    expect("B11 read parent=file (4)", read(r), 4);

    r = req(buf, "sub", LEN("sub"), uji, sizeof(buf), false);
    expect("B12 read folder sub (1)", read(r), 1);

    // read_directory
    struct EXT2DriverRequest d = req(dir_buf, "uji", LEN("uji"), ROOT_INODE, sizeof(dir_buf), true);
    code = read_directory(&d);
    report("B13 read_dir /uji isi benar", code == 0 && dir_has(d.buffer_size, "a.txt", 5) && dir_has(d.buffer_size, "sub", 3), code);
    expect("B14 read_dir file a.txt (1)", read_dir("a.txt", 5, uji), 1);
    expect("B15 read_dir tidak ada (2)", read_dir("nope", 4, uji), 2);
    expect("B16 read_dir parent=file(3)", read_dir("sub", 3, file_a), 3);

    // file besar seperti direct + single + doubly indirect
    for (uint32_t i = 0; i < sizeof(big); i++) big[i] = (uint8_t) (i * 7 + i / BLOCK_SIZE); // pola beda tiap block
    r = req(big, "big.bin", LEN("big.bin"), uji, sizeof(big), false);
    expect("B17 write big.bin 75 KiB", write(&r), 0);

    memset(big_back, 0, sizeof(big_back));
    r = req(big_back, "big.bin", LEN("big.bin"), uji, sizeof(big_back), false);
    code = read(r);
    report("B18 read big.bin + isi", code == 0 && memcmp(big, big_back, sizeof(big)) == 0, code);

    // delete
    r = req(0, "sub", LEN("sub"), uji, 0, true);
    expect("B19 delete sub berisi (2)", delete(r), 2);

    r = req(0, "nope", LEN("nope"), uji, 0, false);
    expect("B20 delete tidak ada (1)", delete(r), 1);

    r = req(0, "a.txt", LEN("a.txt"), file_a, 0, false);
    expect("B21 delete parent=file (3)", delete(r), 3);

    // bersih-bersih dimana semua yang dibuat di atas dihapus lagi
    r = req(0, "a.txt", LEN("a.txt"), sub, 0, false);
    expect("B22 delete /uji/sub/a.txt", delete(r), 0);
    r = req(0, "sub", LEN("sub"), uji, 0, true);
    expect("B23 delete /uji/sub (kosong)", delete(r), 0);
    r = req(0, "big.bin", LEN("big.bin"), uji, 0, false);
    expect("B24 delete big.bin", delete(r), 0);
    r = req(0, "a.txt", LEN("a.txt"), uji, 0, false);
    expect("B25 delete /uji/a.txt", delete(r), 0);

    r = req(buf, "a.txt", LEN("a.txt"), uji, sizeof(buf), false);
    expect("B26 read setelah delete (3)", read(r), 3);

    r = req(0, "uji", LEN("uji"), ROOT_INODE, 0, true);
    expect("B27 delete /uji", delete(r), 0);
    expect("B28 read_dir /uji hilang (2)", read_dir("uji", 3, ROOT_INODE), 2);
}

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_ext2();

    // sample-image.bin berisi file /laur
    bool is_sample = inode_of("laur", LEN("laur"), ROOT_INODE) != 0;
    print_at(0, 0, "Test Chapter 2 OSjur", 0xB);
    print_at(0, 30, is_sample ? "disk: sample-image.bin" : "disk: kosong / buatan sendiri", 0xE);

    if (is_sample) test_sample_image();
    test_crud();

    // ringkasan di baris paling bawah
    char summary[] = "PASS: 00   FAIL: 00";
    summary[6]  = '0' + pass_count / 10;
    summary[7]  = '0' + pass_count % 10;
    summary[17] = '0' + fail_count / 10;
    summary[18] = '0' + fail_count % 10;
    print_at(24, 0, summary, fail_count == 0 ? 0xA : 0xC);
    framebuffer_set_cursor(24, 79);

    while (true);
}