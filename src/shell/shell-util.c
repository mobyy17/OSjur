#include "header/shell/shell.h"

/* Helper kecil untuk bonus: waktu, speaker, tulis di posisi, dan cek process */

uint32_t get_tick(void) {
    uint32_t tick = 0;
    syscall(SYSCALL_GET_TICK, (uint32_t) &tick, 0, 0);
    return tick;
}

void sleep_ms(uint32_t ms) {
    // 1 tick = 1 ms (PIT 1000 Hz). Selama menunggu, process lain tetap dapat giliran.
    uint32_t start = get_tick();
    while (get_tick() - start < ms)
        ;
}

void beep(uint32_t freq, uint32_t ms) {
    syscall(SYSCALL_BEEP, freq, 0, 0);
    sleep_ms(ms);
    syscall(SYSCALL_BEEP, 0, 0, 0);
}

void write_at(uint32_t row, uint32_t col, const char *str, uint32_t color) {
    // Syscall 14 membaca warna dari esi, register yang tidak diisi syscall()
    __asm__ volatile("int $0x30" : : "a"(SYSCALL_WRITE_AT), "b"(row), "c"(col), "d"(str), "S"(color) : "memory");
}

bool process_alive(uint32_t pid) {
    for (uint32_t slot = 0; slot < PROCESS_COUNT_MAX; slot++) {
        struct ProcessInfo info;
        bool found = false;
        syscall(SYSCALL_PS, slot, (uint32_t) &info, (uint32_t) &found);
        if (found && info.pid == pid)
            return true;
    }
    return false;
}
