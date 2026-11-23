#include "header/shell/shell.h"
#include "header/stdlib/string.h"

/* Ring buffer statis: 16 perintah terakhir, tanpa heap */
#define HISTORY_MAX 16

static char     history[HISTORY_MAX][SHELL_LINE_MAX];
static uint32_t history_head  = 0; // slot yang akan diisi berikutnya
static uint32_t history_count = 0;
static uint32_t history_pos   = 0; // 0 = baris baru, 1 = perintah terakhir, dst

static const char *history_at(uint32_t back) {
    // back = 1 artinya entri terbaru
    return history[(history_head + HISTORY_MAX - back) % HISTORY_MAX];
}

void history_push(const char *line) {
    if (line[0] == '\0')
        return;
    if (history_count > 0 && str_eq(history_at(1), line))
        return; // perintah sama berturut-turut cukup disimpan sekali

    uint32_t i = 0;
    for (; line[i] != '\0' && i < SHELL_LINE_MAX - 1; i++)
        history[history_head][i] = line[i];
    history[history_head][i] = '\0';

    history_head = (history_head + 1) % HISTORY_MAX;
    if (history_count < HISTORY_MAX)
        history_count++;
}

void history_reset_cursor(void) {
    history_pos = 0;
}

const char *history_prev(void) {
    if (history_pos >= history_count)
        return 0;
    history_pos++;
    return history_at(history_pos);
}

const char *history_next(void) {
    if (history_pos <= 1) {
        history_pos = 0;
        return 0;
    }
    history_pos--;
    return history_at(history_pos);
}
