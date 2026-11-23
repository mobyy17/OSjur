#include <stdint.h>
#include <stdbool.h>
#include "header/filesystem/ext2.h"

/*
 * Bad Apple!! versi teks. Data frame dibuat tools/badapple-pack.py:
 * tiap frame 80x24 karakter, 1 bit per karakter (1 = putih), 240 byte per frame.
 * Data dipecah ke file ba00, ba01, ... di root karena satu file EXT2 kita maksimal 70 KiB.
 * Shell menunggu program ini (jalan di foreground), Ctrl+C dari shell mematikannya.
 */

#define SYSCALL_READ         0
#define SYSCALL_WRITE_AT     14
#define SYSCALL_GET_TICK     15
#define SYSCALL_CLEAR_SCREEN 16

#define FRAME_W          80
#define FRAME_H          24  // baris 1-24, baris 0 untuk info fps dan clock
#define FRAME_BYTES      (FRAME_W * FRAME_H / 8)
#define FRAMES_PER_CHUNK 280 // harus sama dengan badapple-pack.py
#define CHUNK_BYTES      (FRAMES_PER_CHUNK * FRAME_BYTES)
#define FPS              20  // harus sama dengan badapple-pack.py
#define ROOT_INODE       1

#define BLOCK_CHAR ((char) 0xDB)

static void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("int $0x30" : : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

static void write_at(uint32_t row, uint32_t col, const char *str, uint32_t color) {
    __asm__ volatile("int $0x30" : : "a"(SYSCALL_WRITE_AT), "b"(row), "c"(col), "d"(str), "S"(color) : "memory");
}

static uint32_t get_tick(void) {
    uint32_t tick = 0;
    syscall(SYSCALL_GET_TICK, (uint32_t) &tick, 0, 0);
    return tick;
}

/* Baca file ba<index> ke buf. return false kalau file tidak ada (video habis) */
static bool load_chunk(uint32_t index, uint8_t *buf) {
    char name[5] = {'b', 'a', '0' + index / 10, '0' + index % 10, '\0'};
    struct EXT2DriverRequest request = {
        .buf          = buf,
        .name         = name,
        .name_len     = 4,
        .parent_inode = ROOT_INODE,
        .buffer_size  = CHUNK_BYTES,
        .is_directory = false,
    };
    int8_t retcode = -1;
    syscall(SYSCALL_READ, (uint32_t) &request, (uint32_t) &retcode, 0);
    return retcode == 0;
}

static void draw_frame(const uint8_t *frame) {
    char line[FRAME_W + 1];
    line[FRAME_W] = '\0';
    for (uint32_t row = 0; row < FRAME_H; row++) {
        for (uint32_t col = 0; col < FRAME_W; col++) {
            uint32_t bit = row * FRAME_W + col;
            bool white = frame[bit / 8] & (0x80 >> (bit % 8)); // MSB duluan
            line[col] = white ? BLOCK_CHAR : ' ';
        }
        write_at(row + 1, 0, line, 0xF);
    }
}

static void show_fps(uint32_t fps, uint32_t frame_no) {
    char text[] = "Bad Apple!!  00 fps  frame 0000  (Ctrl+C untuk berhenti)";
    text[13] = '0' + fps / 10 % 10;
    text[14] = '0' + fps % 10;
    text[27] = '0' + frame_no / 1000 % 10;
    text[28] = '0' + frame_no / 100 % 10;
    text[29] = '0' + frame_no / 10 % 10;
    text[30] = '0' + frame_no % 10;
    write_at(0, 0, text, 0x8);
}

int main(void) {
    // Buffer di stack, bukan global: .bss ikut tersimpan di flat binary dan file app jadi besar
    uint8_t chunk[CHUNK_BYTES];

    syscall(SYSCALL_CLEAR_SCREEN, 0, 0, 0);
    if (!load_chunk(0, chunk)) {
        write_at(1, 0, "badapple: data ba00 tidak ada, jalankan make insert-badapple", 0xC);
        return 1;
    }

    uint32_t start         = get_tick();
    uint32_t frame_no      = 0; // nomor frame global
    uint32_t drawn         = 0; // frame yang benar-benar digambar pada detik ini
    uint32_t fps_shown     = 0;
    uint32_t fps_tick      = start;

    for (uint32_t chunk_no = 0; ; chunk_no++) {
        if (chunk_no > 0 && !load_chunk(chunk_no, chunk))
            break;

        for (uint32_t i = 0; i < FRAMES_PER_CHUNK; i++, frame_no++) {
            // Frame ke-n tampil pada start + n * 1000 / FPS ms. Dihitung dari awal supaya tidak drift.
            uint32_t due = start + frame_no * 1000 / FPS;
            uint32_t now = get_tick();
            if (now > due + 1000 / FPS)
                continue; // sudah telat lebih dari satu frame: lewati supaya tetap sinkron
            while (get_tick() < due)
                ;

            draw_frame(&chunk[i * FRAME_BYTES]);
            drawn++;

            if (get_tick() - fps_tick >= 1000) {
                fps_shown = drawn;
                drawn     = 0;
                fps_tick += 1000;
            }
            show_fps(fps_shown, frame_no);
        }
    }

    syscall(SYSCALL_CLEAR_SCREEN, 0, 0, 0);
    return 0;
}
