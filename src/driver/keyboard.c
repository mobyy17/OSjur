#include "header/driver/keyboard.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,

      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

static volatile struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = false,
    .keyboard_input_on  = false,
    .ctrl_pressed       = false,
    .ctrl_c_pending     = false,
    .keyboard_buffer    = 0,
};

void keyboard_state_activate(void) {
    keyboard_state.keyboard_input_on = true;
}

void keyboard_state_deactivate(void) {
    keyboard_state.keyboard_input_on = false;
}

void get_keyboard_buffer(char *buf) {
    *buf = keyboard_state.keyboard_buffer;
    keyboard_state.keyboard_buffer = 0;
}

bool keyboard_is_ctrl_pressed(void) {
    return keyboard_state.ctrl_pressed;
}

bool keyboard_consume_ctrl_c(void) {
    bool pending = keyboard_state.ctrl_c_pending;
    keyboard_state.ctrl_c_pending = false;
    return pending;
}

/* Scancode sesudah prefix 0xE0. Return kode KEY_* atau 0 kalau tidak dipakai. */
static char extended_scancode_to_key(uint8_t scancode) {
    switch (scancode) {
        case EXT_SCANCODE_UP:    return KEY_ARROW_UP;
        case EXT_SCANCODE_DOWN:  return KEY_ARROW_DOWN;
        case EXT_SCANCODE_LEFT:  return KEY_ARROW_LEFT;
        case EXT_SCANCODE_RIGHT: return KEY_ARROW_RIGHT;
        default:                 return 0;
    }
}

void keyboard_isr(void) {
    /* Wajib dibaca di setiap IRQ1, walau input sedang off, supaya buffer controller tidak penuh. */
    uint8_t scancode = in(KEYBOARD_DATA_PORT);

    if (scancode == EXTENDED_SCANCODE_BYTE) {
        keyboard_state.read_extended_mode = true;
        pic_ack(IRQ_KEYBOARD);
        return;
    }

    bool extended = keyboard_state.read_extended_mode;
    keyboard_state.read_extended_mode = false;
    /* Modifier selalu dilacak walau input off. Kalau tidak, Ctrl yang dilepas saat input off akan dianggap masih ditahan setelah input on lagi. */
    if (scancode == SCANCODE_CTRL_PRESS) {
        keyboard_state.ctrl_pressed = true; /* 0x1D = Ctrl kiri, 0xE0 0x1D = Ctrl kanan */
    } else if (scancode == SCANCODE_CTRL_RELEASE) {
        keyboard_state.ctrl_pressed = false;
    } else if (keyboard_state.keyboard_input_on && !(scancode & KEYBOARD_BREAK_BIT)) {
        char key = 0;
        if (extended) {
            key = extended_scancode_to_key(scancode);
        } else if (keyboard_state.ctrl_pressed) {
            /* Hanya Ctrl+C yang diteruskan. Kombinasi Ctrl lain dibuang supaya tidak bentrok dengan KEY_ARROW_* (0x11-0x14 = Ctrl+Q/R/S/T). */
            if (scancode == SCANCODE_C_PRESS) {
                key = KEY_CTRL_C;
                keyboard_state.ctrl_c_pending = true;
            }
        } else {
            key = keyboard_scancode_1_to_ascii_map[scancode];
        }

        if (key != 0) {
            keyboard_state.keyboard_buffer = key;
        }
    }

    /* Tanpa ACK, semua IRQ berikutnya tertahan di PIC. */
    pic_ack(IRQ_KEYBOARD);
}