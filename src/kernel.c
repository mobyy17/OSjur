#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"

static int write_str(int row, int col, const char *s, uint8_t fg) {
    while (*s && col < FRAMEBUFFER_WIDTH) {
        framebuffer_write(row, col++, *s++, fg, 0);
    }
    return col;
}

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);

    int col = 0, row = 0;
    keyboard_state_activate();
    while (true) {
        char c;
        get_keyboard_buffer(&c);
        if (!c) continue;

        switch (c) {
            case '\n':
                col = 0;
                row++;
                break;

            case '\b':
                if (col > 0) {
                    col--;
                    framebuffer_write(row, col, 0x00, 0x7, 0x0);
                }
                break;

            case KEY_ARROW_LEFT:
                if (col > 0) col--;
                break;
            case KEY_ARROW_RIGHT:
                if (col < FRAMEBUFFER_WIDTH - 1) col++;
                break;
            case KEY_ARROW_UP:
                if (row > 0) row--;
                break;
            case KEY_ARROW_DOWN:
                if (row < FRAMEBUFFER_HEIGHT - 1) row++;
                break;

            case KEY_CTRL_C:
                keyboard_consume_ctrl_c();
                write_str(row, col, "^C", 0xC);
                col = 0;
                row++;
                break;

            default:
                framebuffer_write(row, col, c, 0xF, 0);
                col++;
                if (col >= FRAMEBUFFER_WIDTH) {
                    col = 0;
                    row++;
                }
                break;
        }

        if (row >= FRAMEBUFFER_HEIGHT) {
            framebuffer_clear();
            row = 0;
            col = 0;
        }
        framebuffer_set_cursor(row, col);
    }
}