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
 * KeyboardDriverState - Contain all driver states
 *
 * @param read_extended_mode Signaling next read is extended scancode (setelah 0xE0, ex. arrow keys)
 * @param keyboard_input_on  Indicate whether keyboard ISR is activated or not
 * @param ctrl_pressed       Status tombol Ctrl kiri atau kanan sedang ditahan
 * @param ctrl_c_pending     Sticky flag Ctrl+C dengan kondisi bertahan sampai dikonsumsi walau buffer tertimpa
 * @param keyboard_buffer    Storing keyboard input values in ASCII (atau KEY_* di atas)
 */
struct KeyboardDriverState {
    bool read_extended_mode;
    bool keyboard_input_on;
    bool ctrl_pressed;
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
 * ISR tetap membaca scancode, melacak status modifier, dan mengirim ACK, tapi tidak menyimpan karakter.
 */
void keyboard_state_deactivate(void);

/**
 * Menyalin isi keyboard_buffer ke pointer lalu mengosongkan buffer.
 * @param buf Tujuan salinan; diisi 0 kalau tidak ada karakter baru
 */
void get_keyboard_buffer(char *buf);

/**
 * @return true kalau Ctrl (kiri atau kanan) sedang ditahan
 */
bool keyboard_is_ctrl_pressed(void);

/* Cek dan konsumsi Ctrl+C. Mengembalikan true sekali untuk setiap Ctrl+C yang ditekan selama input on lalu flag direset. Dipakai program yang tidak membaca buffer setiap saat supaya Ctrl+C tidak hilang tertimpa tombol lain.  */
bool keyboard_consume_ctrl_c(void);

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Dipanggil main_interrupt_handler() untuk IRQ1. Selalu membaca 1 scancode dari KEYBOARD_DATA_PORT dan selalu mengirim pic_ack(), baik input sedang on maupun off.
 * Status modifier (Ctrl) dan prefix 0xE0 selalu dilacak serta karakter hanya disimpan ke buffer kalau keyboard_input_on bernilai true.
 */
void keyboard_isr(void);

#endif