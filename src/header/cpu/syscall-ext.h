#ifndef _SYSCALL_EXT_H
#define _SYSCALL_EXT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Syscall tambahan untuk bonus (nomor 15 ke atas). Dispatcher di interrupt.c cuma memanggil fungsi-fungsi di sini, logikanya ada di syscall-ext.c.
 *
 * 15 get_tick     ebx = pointer uint32_t, diisi jumlah tick PIT sejak boot (1 tick = 1 ms)
 * 16 clear_screen layar baris 1-24 dikosongkan, cursor kembali ke baris 1 (baris 0 punya clock)
 * 17 shutdown     matikan QEMU lewat port ACPI
 * 18 beep         ebx = frekuensi Hz, 0 = speaker dimatikan
 * 21 set_bg       ebx = warna background shell 0-15
 * 20 exec_args    ebx = pointer request, ecx = pointer int32_t return code, edx = pointer struct ExecArgs
 */
#define SYSCALL_EXT_ARGC_MAX 8
#define SYSCALL_EXT_ARG_LEN  64

/* Layout harus sama dengan struct ExecArgs di header/shell/shell.h */
struct ExecArgs {
    uint32_t argc;
    char     *argv[SYSCALL_EXT_ARGC_MAX];
    uint32_t pid; // diisi kernel: PID process baru kalau berhasil
};

/** Dipanggil timer_isr tiap IRQ0 */
void tick_increment(void);

uint32_t tick_get(void);

/** Matikan mesin. Tidak kembali kalau berhasil. */
void power_shutdown(void);

/**
 * Nyalakan PC speaker dengan frekuensi tertentu lewat PIT channel 2.
 * @param freq Frekuensi Hz, 0 untuk mematikan speaker
 */
void speaker_play(uint32_t freq);

#endif
