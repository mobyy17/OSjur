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

static uint8_t framebuffer_bg = 0x0;

// Posisi cursor untuk framebuffer_putchar(), dipakai output dari syscall.
// Baris 0 disisakan untuk clock, jadi teks shell mulai dari baris 1 dan baris 0 tidak ikut scroll.
static uint8_t cursor_row = 1;
static uint8_t cursor_col = 0;


/* Scrollback (bonus scrollable shell)
 * Baris yang terdorong keluar dari baris 1 waktu layar scroll disimpan di ring buffer.
 * Saat user menggulung ke atas, isi layar sekarang (baris 1-24) disimpan dulu ke live, lalu layar digambar ulang dari gabungan scrollback + live. Baris 0 (clock) tidak ikut. */
#define SCROLLBACK_LINES 200
#define SHELL_ROWS       (FRAMEBUFFER_HEIGHT - 1)

static uint16_t scrollback[SCROLLBACK_LINES][FRAMEBUFFER_WIDTH];
static uint32_t scrollback_head  = 0;
static uint32_t scrollback_count = 0;
static uint32_t view_offset      = 0;
static uint16_t live_screen[SHELL_ROWS][FRAMEBUFFER_WIDTH];

static uint16_t *screen_row(uint8_t row) {
    return (uint16_t*) (FRAMEBUFFER_MEMORY_OFFSET + row * FRAMEBUFFER_WIDTH * 2);
}

static void scrollback_push_row1(void) {
    memcpy(scrollback[scrollback_head], screen_row(1), FRAMEBUFFER_WIDTH * 2);
    scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
    if (scrollback_count < SCROLLBACK_LINES) scrollback_count++;
}

static void scroll_indicator(bool show) {
    const char *text = "[scroll] PgUp/PgDn, tombol lain untuk kembali";
    bool ended = !show;
    for (uint8_t col = 0; col < 72; col++) {
        char c = 0x00;
        if (!ended && text[col] == '\0') ended = true;
        if (!ended) c = text[col];
        framebuffer_write(0, col, c, 0xE, framebuffer_bg);
    }
}

void framebuffer_scroll_reset(void) {
    if (view_offset == 0)
        return;
    memcpy(screen_row(1), live_screen, sizeof(live_screen));
    view_offset = 0;
    scroll_indicator(false);
}

void framebuffer_scroll_view(int32_t delta) {
    if (delta == 0) {
        framebuffer_scroll_reset();
        framebuffer_set_cursor(cursor_row, cursor_col);
        return;
    }
    if (view_offset == 0 && delta > 0)
        memcpy(live_screen, screen_row(1), sizeof(live_screen));

    int32_t offset = (int32_t) view_offset + delta;
    if (offset < 0) offset = 0;
    if (offset > (int32_t) scrollback_count) offset = (int32_t) scrollback_count;
    if (view_offset == 0 && offset == 0)
        return; // belum ada yang bisa digulung

    if (offset == 0) {
        framebuffer_scroll_reset();
        framebuffer_set_cursor(cursor_row, cursor_col);
        return;
    }
    view_offset = (uint32_t) offset;

    // Baris gabungan ke-i: i < count dari scrollback (lama ke baru), sisanya dari live_screen
    uint32_t first = scrollback_count - view_offset;
    for (uint32_t r = 0; r < SHELL_ROWS; r++) {
        uint32_t i = first + r;
        uint16_t *src;
        if (i < scrollback_count)
            src = scrollback[(scrollback_head + SCROLLBACK_LINES - scrollback_count + i) % SCROLLBACK_LINES];
        else
            src = live_screen[i - scrollback_count];
        memcpy(screen_row((uint8_t) (r + 1)), src, FRAMEBUFFER_WIDTH * 2);
    }
    scroll_indicator(true);
    framebuffer_set_cursor(FRAMEBUFFER_HEIGHT, 0); // sembunyikan cursor selama menggulung
}

void framebuffer_putchar(char c, uint8_t color) {
    framebuffer_scroll_reset();
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            framebuffer_write(cursor_row, cursor_col, 0x00, 0x7, framebuffer_bg);
        }
    } else if (c >= ' ') {
        framebuffer_write(cursor_row, cursor_col, c, color, framebuffer_bg);
        cursor_col++;
        if (cursor_col >= FRAMEBUFFER_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= FRAMEBUFFER_HEIGHT) {
        scrollback_push_row1();
        memmove(FRAMEBUFFER_MEMORY_OFFSET + FRAMEBUFFER_WIDTH * 2, FRAMEBUFFER_MEMORY_OFFSET + FRAMEBUFFER_WIDTH * 2 * 2, (FRAMEBUFFER_HEIGHT - 2) * FRAMEBUFFER_WIDTH * 2);
        for (uint8_t col = 0; col < FRAMEBUFFER_WIDTH; col++)
            framebuffer_write(FRAMEBUFFER_HEIGHT - 1, col, 0x00, 0x7, framebuffer_bg);
        cursor_row = FRAMEBUFFER_HEIGHT - 1;
    }
    framebuffer_set_cursor(cursor_row, cursor_col);
}

void framebuffer_puts(const char *str, uint32_t count, uint8_t color) {
    for (uint32_t i = 0; i < count; i++)
        framebuffer_putchar(str[i], color);
}
void framebuffer_clear_shell(void) {
    framebuffer_scroll_reset();
    for (uint8_t col = 0; col < 72; col++)
        framebuffer_write(0, col, 0x00, 0x7, framebuffer_bg);
    for (uint8_t row = 1; row < FRAMEBUFFER_HEIGHT; row++)
        for (uint8_t col = 0; col < FRAMEBUFFER_WIDTH; col++)
            framebuffer_write(row, col, 0x00, 0x7, framebuffer_bg);
    cursor_row = 1;
    cursor_col = 0;
    framebuffer_set_cursor(cursor_row, cursor_col);
}

uint8_t framebuffer_get_background(void) {
    return framebuffer_bg;
}

void framebuffer_set_background(uint8_t bg) {
    // Matikan mode blink di attribute controller VGA (register 0x10 bit 3), supaya bit ke-4 atribut dipakai sebagai warna bg terang.
    in(0x3DA);                      // reset flip-flop index/data port 0x3C0
    out(0x3C0, 0x10 | 0x20);        // index 0x10, bit 5 = tetap tampilkan layar
    uint8_t mode = in(0x3C1);
    out(0x3C0, mode & ~0x08);
    out(0x3C0, 0x20);
    out(0x3C8, 2);
    out(0x3C9, 14); // R ~ 56
    out(0x3C9, 22); // G ~ 88
    out(0x3C9, 8); // B ~ 32

    // Sel yang bg-nya masih warna lama ikut diganti, jadi isi layar tidak hilang
    uint8_t old = framebuffer_bg;
    framebuffer_bg = bg & 0xF;
    for (uint32_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        uint8_t attr = FRAMEBUFFER_MEMORY_OFFSET[i * 2 + 1];
        if ((attr >> 4) == old)
            FRAMEBUFFER_MEMORY_OFFSET[i * 2 + 1] = (framebuffer_bg << 4) | (attr & 0xF);
    }
}
