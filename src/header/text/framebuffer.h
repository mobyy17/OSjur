#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FRAMEBUFFER_MEMORY_OFFSET ((uint8_t*) 0xC00B8000)
#define CURSOR_PORT_CMD    0x03D4
#define CURSOR_PORT_DATA   0x03D5

#define FRAMEBUFFER_WIDTH  80
#define FRAMEBUFFER_HEIGHT 25

/**
 * Terminal framebuffer
 * Resolution: 80x25
 * Starting at FRAMEBUFFER_MEMORY_OFFSET,
 * - Even number memory: Character, 8-bit
 * - Odd number memory:  Character color lower 4-bit, Background color upper 4-bit
*/

/**
 * Set framebuffer character and color with corresponding parameter values.
 * More details: https://en.wikipedia.org/wiki/BIOS_color_attributes
 *
 * @param row Vertical location (index start 0)
 * @param col Horizontal location (index start 0)
 * @param c   Character
 * @param fg  Foreground / Character color
 * @param bg  Background color
 */
void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg);

/**
 * Set cursor to specified location. Row and column starts from 0
 * 
 * @param r row
 * @param c column
*/
void framebuffer_set_cursor(uint8_t r, uint8_t c);

/**
 * Set all cell in framebuffer character to 0x00 (empty character)
 * and color to 0x07 (gray character & black background)
 * Extra note: It's allowed to use different color palette for this
 *
 */
void framebuffer_clear(void);

/**
 * Tulis satu karakter di posisi cursor lalu geser cursor (dipakai syscall putchar).
 * '\n' pindah baris, '\b' hapus satu karakter ke kiri, karakter kontrol lain diabaikan.
 * Kalau sudah di baris terakhir, layar di-scroll naik satu baris.
 *
 * @param c     Character
 * @param color Warna karakter (foreground), background hitam
 */
void framebuffer_putchar(char c, uint8_t color);

/**
 * Tulis count karakter dari str dengan framebuffer_putchar() (dipakai syscall puts).
 *
 * @param str   Pointer ke buffer karakter
 * @param count Jumlah karakter yang ditulis
 * @param color Warna karakter (foreground), background hitam
 */
void framebuffer_puts(const char *str, uint32_t count, uint8_t color);

/**
 * Kosongkan baris 1 sampai 24 dan kembalikan cursor ke awal baris 1.
 * Di baris 0 hanya kolom 0-71 yang dihapus, kolom 72-79 dipakai program clock.
 */
void framebuffer_clear_shell(void);

/**
 * Ganti warna background shell (0-15). Mode blink VGA dimatikan supaya 16 warna bg bisa dipakai.
 * Sel yang bg nya masih warna lama ikut diwarnai ulang.
 */
void framebuffer_set_background(uint8_t bg);

/** @return Warna background shell sekarang */
uint8_t framebuffer_get_background(void);

/**
 * Gulung tampilan shell (bonus scrollable shell). Baris 0 (clock) tidak ikut.
 * @param delta Jumlah baris, positif = ke atas (lebih lama), negatif = ke bawah, 0 = kembali ke paling bawah
 */
void framebuffer_scroll_view(int32_t delta);

/** Kembali ke tampilan normal kalau sedang digulung */
void framebuffer_scroll_reset(void);

#endif