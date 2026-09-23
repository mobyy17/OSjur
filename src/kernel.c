#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"

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

        if (c == '\n') {
            col = 0;
            row++;
        } else if (c == '\b') {
            if (col > 0) {
                col--;
                framebuffer_write(row, col, 0x00, 0x7, 0x0);
            }
        } else {
            framebuffer_write(row, col, c, 0xF, 0);
            col++;
            if (col >= FRAMEBUFFER_WIDTH) {
                col = 0;
                row++;
            }
        }

        if (row >= FRAMEBUFFER_HEIGHT) {
            framebuffer_clear();
            row = 0;
            col = 0;
        }
        framebuffer_set_cursor(row, col);
    }
}