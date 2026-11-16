#include "header/shell/shell.h"

static void out_uint(uint32_t value) {
    char digits[10];
    uint32_t len = 0;
    do {
        digits[len++] = '0' + value % 10;
        value /= 10;
    } while (value > 0);
    while (len > 0)
        out_putchar(digits[--len]);
}

/* Tambah spasi sampai kolom berikutnya rata */
static void out_pad(uint32_t written, uint32_t width) {
    for (uint32_t i = written; i < width; i++)
        out_putchar(' ');
}

static uint32_t uint_len(uint32_t value) {
    uint32_t len = 1;
    while (value >= 10) {
        value /= 10;
        len++;
    }
    return len;
}

/* ps: tampilkan semua process aktif (PID, nama, state) */
void cmd_ps(int argc, char *argv[]) {
    (void) argv;
    if (argc != 1) {
        out_puts("pemakaian: ps\n");
        return;
    }

    out_puts("PID   NAMA                             STATE\n");
    for (uint32_t slot = 0; slot < PROCESS_COUNT_MAX; slot++) {
        struct ProcessInfo info;
        bool active = false;
        syscall(SYSCALL_PS, slot, (uint32_t) &info, (uint32_t) &active);
        if (!active)
            continue;

        out_uint(info.pid);
        out_pad(uint_len(info.pid), 6);

        // name dijamin null-terminated oleh kernel, tapi tetap dibatasi panjang array
        uint32_t name_len = 0;
        while (name_len < PROCESS_NAME_LENGTH_MAX && info.name[name_len] != '\0')
            name_len++;
        out_write(info.name, name_len);
        out_pad(name_len, 33);

        if (info.state == PROCESS_STATE_READY)
            out_puts("READY\n");
        else if (info.state == PROCESS_STATE_RUNNING)
            out_puts("RUNNING\n");
        else if (info.state == PROCESS_STATE_WAITING)
            out_puts("WAITING\n");
        else
            out_puts("?\n");
    }
}
