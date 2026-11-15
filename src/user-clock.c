#include <stdint.h>
#include <stdbool.h>

/* Jam di pojok kanan bawah layar. Ditulis lewat syscall 14 (tulis di posisi),
 * jadi kursor shell tidak bergeser. Jalan terus sampai di-kill. */

#define SYSCALL_GET_TIME     9
#define SYSCALL_WRITE_AT    14

#define CLOCK_ROW           24
#define CLOCK_COL           72   // "HH:MM:SS" = 8 karakter, pas sampai kolom 79
#define CLOCK_COLOR         0xE  // kuning
#define TIMEZONE_OFFSET     7    // RTC QEMU memberi UTC, ditampilkan sebagai WIB

/* Layout harus sama dengan struct ClockTime di header/driver/cmos.h */
struct ClockTime {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} __attribute__((packed));

static void get_time(struct ClockTime *time) {
    __asm__ volatile("int $0x30" : : "a"(SYSCALL_GET_TIME), "b"(time) : "memory");
}

/* Syscall 14 membaca warna dari esi, register yang tidak diisi syscall() milik shell. */
static void write_at(uint32_t row, uint32_t col, const char *str, uint32_t color) {
    __asm__ volatile("int $0x30"
                     :
                     : "a"(SYSCALL_WRITE_AT), "b"(row), "c"(col), "d"(str), "S"(color)
                     : "memory");
}

static void put_two_digits(char *dst, uint8_t value) {
    dst[0] = '0' + value / 10;
    dst[1] = '0' + value % 10;
}

int main(void) {
    char text[9] = "00:00:00";
    struct ClockTime now;
    uint8_t last_second = 0xFF;

    while (true) {
        get_time(&now);
        // Tulis hanya saat detik berubah supaya tidak membanjiri kernel dengan syscall
        if (now.second == last_second)
            continue;
        last_second = now.second;

        put_two_digits(&text[0], (uint8_t) ((now.hour + TIMEZONE_OFFSET) % 24));
        put_two_digits(&text[3], now.minute);
        put_two_digits(&text[6], now.second);
        write_at(CLOCK_ROW, CLOCK_COL, text, CLOCK_COLOR);
    }
    return 0;
}
