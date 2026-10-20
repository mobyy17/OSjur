#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/cpu/interrupt.h"

#define EXT_SCANCODE_UP        0x48
#define EXT_SCANCODE_DOWN      0x50
#define EXT_SCANCODE_LEFT      0x4B
#define EXT_SCANCODE_RIGHT     0x4D
#define SCANCODE_CTRL_PRESS    0x1D
#define SCANCODE_CTRL_RELEASE  0x9D
#define SCANCODE_LSHIFT_PRESS   0x2A
#define SCANCODE_LSHIFT_RELEASE 0xAA
#define SCANCODE_RSHIFT_PRESS   0x36
#define SCANCODE_RSHIFT_RELEASE 0xB6
#define SCANCODE_C_PRESS       0x2E

#define KEYBOARD_DATA_PORT     0x60
#define EXTENDED_SCANCODE_BYTE 0xE0
#define KEYBOARD_BREAK_BIT     0x80
#define KEY_CTRL_C             0x03
#define KEY_ARROW_UP           0x11
#define KEY_ARROW_DOWN         0x12
#define KEY_ARROW_LEFT         0x13
#define KEY_ARROW_RIGHT        0x14

/**
 * keyboard_scancode_1_to_ascii_map[256], Convert scancode values that correspond to ASCII printables
 * How to use this array: ascii_char = k[scancode]
 *
 * By default, QEMU using scancode set 1 (from empirical testing)
 */
extern const char keyboard_scancode_1_to_ascii_map[256];

/**
 * Sama seperti map di atas, tapi untuk tombol yang ditekan sambil menahan Shift (huruf besar dan simbol seperti | _ " : yang nanti dibutuhkan shell).
 */
extern const char keyboard_scancode_1_to_ascii_shift_map[256];

/**
 * KeyboardDriverState - Contain all driver states
 *
 * @param read_extended_mode Signaling next read is extended scancode (setelah 0xE0, ex. arrow keys)
 * @param keyboard_input_on  Indicate whether keyboard ISR is activated or not
 * @param ctrl_pressed       Status tombol Ctrl kiri atau kanan sedang ditahan
 * @param shift_pressed      Status tombol Shift kiri atau kanan sedang ditahan
 * @param ctrl_c_pending     Flag Ctrl+C, tetap true sampai dibaca lewat keyboard_consume_ctrl_c()
 * @param keyboard_buffer    Storing keyboard input values in ASCII (atau KEY_* di atas)
 */
struct KeyboardDriverState {
    bool read_extended_mode;
    bool keyboard_input_on;
    bool ctrl_pressed;
    bool shift_pressed;
    bool ctrl_c_pending;
    char keyboard_buffer;
} __attribute((packed));

/**
 * Menyalakan pembacaan input keyboard.
 * Setelah dipanggil, ISR mulai menyimpan karakter ke buffer.
 */
void keyboard_state_activate(void);

/**
 * Mematikan pembacaan input keyboard.
 * ISR tetap membaca scancode, melacak status Ctrl/Shift, dan mengirim ACK, tapi tidak menyimpan karakter.
 */
void keyboard_state_deactivate(void);

/**
 * Menyalin isi keyboard_buffer ke pointer lalu mengosongkan buffer.
 * @param buf Tujuan salinan; diisi 0 kalau tidak ada karakter baru
 */
void get_keyboard_buffer(char *buf);

/**
 * Cek dan reset flag Ctrl+C.
 * @return true kalau Ctrl+C ditekan sejak terakhir kali fungsi ini dipanggil
 */
bool keyboard_consume_ctrl_c(void);

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Dipanggil main_interrupt_handler() untuk IRQ1. Selalu membaca 1 scancode dari KEYBOARD_DATA_PORT
 * dan selalu mengirim pic_ack(), baik input sedang on maupun off.
 */
void keyboard_isr(void);

#endif