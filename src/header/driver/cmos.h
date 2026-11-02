#ifndef _CMOS_H
#define _CMOS_H

#include <stdint.h>

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT    0x71

#define CMOS_REG_SECOND   0x00
#define CMOS_REG_MINUTE   0x02
#define CMOS_REG_HOUR     0x04
#define CMOS_REG_STATUS_A 0x0A
#define CMOS_REG_STATUS_B 0x0B

/**
 * Waktu dari RTC CMOS. Dipakai juga oleh user program lewat syscall 9 (get time), jadi layout struct ini harus sama di kernel dan user.
 *
 * @param hour   Jam 0-23 (UTC, sesuai jam RTC QEMU default)
 * @param minute Menit 0-59
 * @param second Detik 0-59
 */
struct ClockTime {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} __attribute__((packed));

/**
 * Baca jam, menit, detik dari RTC CMOS
 *
 * @param time Tempat menyimpan hasil
 */
void cmos_read_time(struct ClockTime *time);

#endif
