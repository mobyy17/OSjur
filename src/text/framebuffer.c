#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    uint16_t pos = r * FRAMEBUFFER_WIDTH + c;

    // Register 0x0F = byte low posisi cursor, 0x0E = byte high
    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t) (pos & 0xFF));
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    // Tiap sel = 2 byte: [karakter][atribut], atribut = bg di nibble atas, fg di nibble bawah
    uint16_t offset = (row * FRAMEBUFFER_WIDTH + col) * 2;
    FRAMEBUFFER_MEMORY_OFFSET[offset]     = c;
    FRAMEBUFFER_MEMORY_OFFSET[offset + 1] = (bg << 4) | fg;
}

void framebuffer_clear(void) {
    // Karakter 0x00, atribut 0x07 (fg abu-abu, bg hitam) sesuai doc di header
    for (uint8_t row = 0; row < FRAMEBUFFER_HEIGHT; row++) {
        for (uint8_t col = 0; col < FRAMEBUFFER_WIDTH; col++) {
            framebuffer_write(row, col, 0x00, 0x7, 0x0);
        }
    }
}