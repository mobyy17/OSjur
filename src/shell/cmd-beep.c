#include "header/shell/shell.h"

static uint32_t parse_uint(const char *s) {
    uint32_t value = 0;
    for (uint32_t i = 0; s[i] >= '0' && s[i] <= '9'; i++)
        value = value * 10 + (uint32_t) (s[i] - '0');
    return value;
}

/* beep [frekuensi] [durasi ms], default 440 Hz selama 300 ms.
 * "beep tune" memainkan nada pendek. */
void cmd_beep(int argc, char *argv[]) {
    if (argc >= 2 && str_eq(argv[1], "tune")) {
        static const uint32_t notes[] = {523, 587, 659, 698, 784, 880, 988, 1047};
        for (uint32_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++)
            beep(notes[i], 150);
        return;
    }

    uint32_t freq = argc >= 2 ? parse_uint(argv[1]) : 440;
    uint32_t ms   = argc >= 3 ? parse_uint(argv[2]) : 300;
    if (freq == 0 || ms == 0) {
        out_puts("pemakaian: beep [frekuensi] [durasi ms] atau beep tune\n");
        return;
    }
    if (ms > 5000)
        ms = 5000;
    beep(freq, ms);
}
