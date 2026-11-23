#include "header/shell/shell.h"

#define HELM_W   26
#define HELM_H   25
#define PAD      1
#define PIX_W    (HELM_W + 2 * PAD)   // 28
#define PIX_H    28                   // 2 piksel per baris teks
#define ROWS     (PIX_H / 2)          // 14 baris teks
#define TOP_ROW  1                    // baris 0 milik clock
#define INFO_COL 34

/* Grid dikonversi dari gambar helm buatan kelompok (pixel art 26x25, palet VGA). */
static const char *helm[HELM_H] = {
    ".........RRRRRRRR.........",
    "......RRRrrrrrrrrRR.......",
    ".....RrrrrrrrrrrrrrRR.....",
    "...RRrrrrrrrrrrrrrrrrRR...",
    "..Rrrrrrrrrr.rrrrrrrrrrR..",
    "..rrrrrR.........rrrrrrrR.",
    ".rrrrR..R..R..RR...rrrrR..",
    ".rrrR...rR.rR.R.....rR....",
    ".rrrR....R................",
    ".rrR......YYYYYYYY........",
    ".rrR.....YBBBBBBBBY.......",
    ".rrR....YBBBBBBBBY.YY.....",
    ".rrR....GBBBBBBY.YYYY.....",
    ".rrR....GBY...YYYYYYYY....",
    "..rR....GY.YYYYBBBYYYY....",
    "..rrR...GY.BYYY...BBBY....",
    "...rR...GBYBYYY......Y....",
    "....rR...GY.BYYY.....Y....",
    ".....R...GY.BYYYYY...Y....",
    ".....R...GYY.BYYYYYY.Y....",
    "....RR...GY...BYYYYYY.....",
    "....R....GY....BYYYYY.....",
    "...r....GBY....BBYYYY.....",
    "..r.....GY.......BYYY.....",
    ".r................BBY.....",
};

static uint8_t pixel[PIX_H][PIX_W]; // 0 = kosong (hitam)

static uint8_t helm_color(char c) {
    switch (c) {
        case 'R': return 0xC; // merah terang
        case 'r': return 0x4; // merah gelap
        case 'Y': return 0xE; // kuning
        case 'B': return 0x6; // coklat
        case 'W': return 0xF; // putih
        case 'G': return 0x8; // abu gelap
        default:  return 0;
    }
}

static void build_pixels(void) {
    for (uint32_t y = 0; y < HELM_H; y++)
        for (uint32_t x = 0; x < HELM_W; x++)
            pixel[y + PAD][x + PAD] = helm_color(helm[y][x]);
}

/* Gambar satu baris teks */
static void draw_helm_row(uint32_t row) {
    for (uint32_t col = 0; col < PIX_W; col++) {
        uint8_t top    = pixel[row * 2][col];
        uint8_t bottom = pixel[row * 2 + 1][col];
        char cell[2] = {(char) 0xDF, '\0'};
        uint8_t fg = top, bg = bottom;
        if (top == 0 && bottom == 0) {
            cell[0] = ' ';
        } else if (top == 0) {
            cell[0] = (char) 0xDC;
            fg = bottom;
            bg = 0;
        } else if (top == bottom) {
            cell[0] = (char) 0xDB;
        }
        write_at(TOP_ROW + row, col + 1, cell, (uint32_t) ((bg << 4) | fg));
    }
}

#define COLOR_LABEL 0xC
#define COLOR_VALUE 0xF

/* Tulis angka desimal ke buf, return pointer ke akhir string */
static char *put_uint(char *buf, uint32_t value) {
    char tmp[11];
    uint32_t n = 0;
    do {
        tmp[n++] = '0' + value % 10;
        value /= 10;
    } while (value > 0);
    while (n > 0)
        *buf++ = tmp[--n];
    *buf = '\0';
    return buf;
}

static char *put_str(char *buf, const char *s) {
    while (*s)
        *buf++ = *s++;
    *buf = '\0';
    return buf;
}

/* Nama CPU dari instruksi cpuid (leaf 0x80000002-4). cpuid boleh dipanggil dari ring 3. */
static void cpu_name(char *buf) {
    uint32_t max_leaf;
    __asm__ volatile("cpuid" : "=a"(max_leaf) : "a"(0x80000000) : "ebx", "ecx", "edx");
    if (max_leaf < 0x80000004) {
        put_str(buf, "x86 (cpuid tidak lengkap)");
        return;
    }
    uint32_t *out = (uint32_t*) buf;
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++, out += 4)
        __asm__ volatile("cpuid" : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3]) : "a"(leaf));
    buf[48] = '\0';

    char *start = buf;
    while (*start == ' ')
        start++;
    uint32_t i = 0;
    for (; start[i] != '\0' && i < 40; i++)
        buf[i] = start[i];
    buf[i] = '\0';
}

static uint32_t count_processes(void) {
    uint32_t count = 0;
    for (uint32_t slot = 0; slot < PROCESS_COUNT_MAX; slot++) {
        struct ProcessInfo info;
        bool found = false;
        syscall(SYSCALL_PS, slot, (uint32_t) &info, (uint32_t) &found);
        if (found)
            count++;
    }
    return count;
}

static uint32_t count_programs(void) {
    uint8_t table[SHELL_DIR_MAX];
    uint32_t size = dir_read_table(ROOT_INODE, table);
    uint32_t offset = 0, count = 0;
    struct EXT2DirectoryEntry *entry;
    while ((entry = dir_next_entry(table, size, &offset)) != 0)
        if (entry->file_type == EXT2_FT_REG_FILE)
            count++;
    return count;
}

static void info_line(uint32_t *row, const char *label, const char *value) {
    write_at(*row, INFO_COL, label, COLOR_LABEL);
    write_at(*row, INFO_COL + str_len(label), ": ", COLOR_VALUE);
    write_at(*row, INFO_COL + str_len(label) + 2, value, COLOR_VALUE);
    (*row)++;
}

static void draw_info(void) {
    char buf[64];
    uint32_t row = TOP_ROW + 2;

    write_at(row, INFO_COL, "root", COLOR_LABEL);
    write_at(row, INFO_COL + 4, "@", COLOR_VALUE);
    write_at(row, INFO_COL + 5, "osjur", COLOR_LABEL);
    row++;
    write_at(row++, INFO_COL, "----------", COLOR_VALUE);

    info_line(&row, "OS", "OSjur i386 (x86 protected mode)");
    info_line(&row, "Kernel", "osjur 1.1 higher half");

    uint32_t seconds = get_tick() / 1000;
    char *p = put_uint(buf, seconds / 60);
    p = put_str(p, " min ");
    p = put_uint(p, seconds % 60);
    put_str(p, " detik");
    info_line(&row, "Uptime", buf);

    put_str(put_uint(buf, count_programs()), " (root)");
    info_line(&row, "Programs", buf);
    info_line(&row, "Shell", "osjur-sh");
    info_line(&row, "Terminal", "VGA text 80x25");
    cpu_name(buf);
    info_line(&row, "CPU", buf);
    info_line(&row, "Memory", "4 MiB per proses");
    info_line(&row, "Disk", "EXT2-IF2130 4 MiB");
    put_str(put_uint(buf, count_processes()), " berjalan");
    info_line(&row, "Proses", buf);
}

void banner_show(bool animate) {
    static bool built = false;
    if (!built) {
        build_pixels();
        built = true;
    }

    for (uint32_t row = 0; row < ROWS; row++) {
        draw_helm_row(row);
        if (animate)
            sleep_ms(45);
    }
    draw_info();
    if (animate) {
        beep(523, 80);
        beep(784, 120);
    }

    for (uint32_t i = 0; i < ROWS; i++)
        print("\n", COLOR_TEXT);
    print("Ketik 'help' untuk daftar perintah.\n", 0x7);
}
