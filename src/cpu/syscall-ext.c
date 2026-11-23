#include "header/cpu/syscall-ext.h"
#include "header/cpu/portio.h"

// Timer cuma jalan saat CPU di user mode (kernel berjalan dengan IF = 0), jadi waktu yang habis di dalam syscall tidak ikut terhitung. Untuk pacing video sudah cukup.
static volatile uint32_t timer_tick = 0;

void tick_increment(void) {
    timer_tick++;
}

uint32_t tick_get(void) {
    return timer_tick;
}

void power_shutdown(void) {
    // QEMU baru (machine pc + SeaBIOS): port PM1a_CNT di 0x604, nilai 0x2000 = SLP_EN dengan S5
    out16(0x604, 0x2000);
    // QEMU lama dan Bochs pakai port 0xB004
    out16(0xB004, 0x2000);
    // Kalau masih hidup juga, berhenti di sini saja
    while (true)
        __asm__ volatile("cli; hlt");
}

// ini buat speaker
#define PIT_BASE_FREQUENCY 1193180
#define PIT_CHANNEL_2_DATA 0x42
#define PIT_COMMAND        0x43
#define SPEAKER_PORT       0x61

void speaker_play(uint32_t freq) {
    if (freq == 0) {
        // bit 0 (gate PIT channel 2) dan bit 1 (data speaker) dimatikan
        out(SPEAKER_PORT, in(SPEAKER_PORT) & 0xFC);
        return;
    }
    if (freq < 20) freq = 20;
    if (freq > 20000) freq = 20000;

    uint32_t divisor = PIT_BASE_FREQUENCY / freq;
    // 0xB6 = channel 2, lobyte/hibyte, mode 3 (square wave). Channel 0 milik scheduler tidak disentuh.
    out(PIT_COMMAND, 0xB6);
    out(PIT_CHANNEL_2_DATA, (uint8_t) (divisor & 0xFF));
    out(PIT_CHANNEL_2_DATA, (uint8_t) ((divisor >> 8) & 0xFF));

    uint8_t state = in(SPEAKER_PORT);
    if ((state & 0x3) != 0x3) out(SPEAKER_PORT, state | 0x3);
}
