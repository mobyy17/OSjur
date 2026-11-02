#include "header/driver/cmos.h"
#include "header/cpu/portio.h"

// Pilih register lewat port 0x70, lalu baca isinya dari port 0x71
static uint8_t cmos_read_register(uint8_t reg) {
    out(CMOS_ADDRESS_PORT, reg);
    return in(CMOS_DATA_PORT);
}

// BCD: 0x59 artinya 59 (nibble atas = puluhan, nibble bawah = satuan)
static uint8_t bcd_to_binary(uint8_t value) {
    return (value >> 4) * 10 + (value & 0x0F);
}

void cmos_read_time(struct ClockTime *time) {
    // Bit 7 status A = RTC sedang update.
    while (cmos_read_register(CMOS_REG_STATUS_A) & 0x80);

    uint8_t second = cmos_read_register(CMOS_REG_SECOND);
    uint8_t minute = cmos_read_register(CMOS_REG_MINUTE);
    uint8_t hour   = cmos_read_register(CMOS_REG_HOUR);

    // Bit 2 status B = 1 berarti sudah biner, 0 berarti BCD
    // Format 12 jam tidak ditangani karena RTC QEMU default 24 jam
    if (!(cmos_read_register(CMOS_REG_STATUS_B) & 0x04)) {
        second = bcd_to_binary(second);
        minute = bcd_to_binary(minute);
        hour   = bcd_to_binary(hour);
    }

    time->hour   = hour;
    time->minute = minute;
    time->second = second;
}
