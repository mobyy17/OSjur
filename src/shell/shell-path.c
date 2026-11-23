#include "header/shell/shell.h"

/*
 * PATH sebagai array dinamis di heap (bonus PATH array dinamis).
 * path_list menunjuk ke array char* hasil malloc. Kalau penuh, dibuat array baru
 * 2x lebih besar, isinya disalin, lalu array lama di-free. Tiap string folder juga
 * disalin ke heap, jadi jumlah dan panjang entri tidak dibatasi array statis.
 */
static char   **path_list  = 0;
static uint32_t path_count = 0;
static uint32_t path_cap   = 0;

static char *str_dup(const char *s, uint32_t len) {
    char *copy = malloc(len + 1);
    if (copy == 0)
        return 0;
    for (uint32_t i = 0; i < len; i++)
        copy[i] = s[i];
    copy[len] = '\0';
    return copy;
}

static bool path_push(const char *dir, uint32_t len) {
    if (path_count == path_cap) {
        uint32_t new_cap = path_cap == 0 ? 2 : path_cap * 2;
        char **grown = malloc(new_cap * sizeof(char*));
        if (grown == 0)
            return false;
        for (uint32_t i = 0; i < path_count; i++)
            grown[i] = path_list[i];
        free(path_list);
        path_list = grown;
        path_cap  = new_cap;
    }
    char *copy = str_dup(dir, len);
    if (copy == 0)
        return false;
    path_list[path_count++] = copy;
    return true;
}

static void path_clear(void) {
    for (uint32_t i = 0; i < path_count; i++)
        free(path_list[i]);
    free(path_list);
    path_list  = 0;
    path_count = path_cap = 0;
}

/* PATH awal "/" supaya aplikasi di root tetap bisa dipanggil dari folder mana pun */
static void path_init(void) {
    if (path_list == 0)
        path_push("/", 1);
}

/* Ubah path folder (absolut atau relatif dari cwd, boleh bertingkat) jadi inode */
static bool resolve_dir(const char *path, uint32_t *inode) {
    uint32_t cur = (path[0] == '/') ? ROOT_INODE : cwd_inode;
    uint32_t i = 0;
    while (path[i] != '\0') {
        while (path[i] == '/')
            i++;
        if (path[i] == '\0')
            break;
        char name[SHELL_LINE_MAX];
        uint32_t len = 0;
        while (path[i] != '\0' && path[i] != '/' && len < SHELL_LINE_MAX - 1)
            name[len++] = path[i++];
        name[len] = '\0';
        bool is_dir = false;
        if (!dir_lookup(cur, name, &cur, &is_dir) || !is_dir)
            return false;
    }
    *inode = cur;
    return true;
}

bool path_find_program(const char *name, uint32_t *parent) {
    path_init();
    for (uint32_t i = 0; i < path_count; i++) {
        uint32_t dir = 0, inode = 0;
        bool is_dir = false;
        if (resolve_dir(path_list[i], &dir) && dir_lookup(dir, name, &inode, &is_dir) && !is_dir) {
            *parent = dir;
            return true;
        }
    }
    return false;
}

/* export PATH=/a:/b       ganti seluruh PATH
 * export PATH=$PATH:/c    tambahkan ke PATH yang sudah ada */
void cmd_export(int argc, char *argv[]) {
    const char *prefix = "PATH=";
    char *arg = argc == 2 ? argv[1] : 0;
    for (uint32_t i = 0; arg != 0 && i < 5; i++)
        if (arg[i] != prefix[i])
            arg = 0;
    if (arg == 0) {
        out_puts("pemakaian: export PATH=/folder1:/folder2 atau export PATH=$PATH:/folder\n");
        return;
    }
    char *value = arg + 5;

    path_init();
    const char *keep = "$PATH";
    bool append = true;
    for (uint32_t i = 0; i < 5; i++)
        if (value[i] != keep[i])
            append = false;
    if (append)
        value += 5;
    else
        path_clear();

    uint32_t start = 0;
    for (uint32_t i = 0;; i++) {
        if (value[i] != ':' && value[i] != '\0')
            continue;
        if (i > start) {
            uint32_t dir = 0;
            char saved = value[i];
            value[i] = '\0';
            if (!resolve_dir(&value[start], &dir)) {
                out_puts("export: peringatan, folder ");
                out_puts(&value[start]);
                out_puts(" belum ada\n");
            }
            value[i] = saved;
            if (!path_push(&value[start], i - start)) {
                out_puts("export: heap penuh\n");
                return;
            }
        }
        if (value[i] == '\0')
            break;
        start = i + 1;
    }
}

/* path: dump isi array dinamis PATH beserta alamatnya di heap */
void cmd_path(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    path_init();
    out_puts("PATH array di ");
    out_hex32((uint32_t) path_list);
    out_puts(", count ");
    out_dec(path_count);
    out_puts(", capacity ");
    out_dec(path_cap);
    out_puts("\n");
    for (uint32_t i = 0; i < path_count; i++) {
        out_puts("  [");
        out_dec(i);
        out_puts("] ");
        out_hex32((uint32_t) path_list[i]);
        out_puts(" \"");
        out_puts(path_list[i]);
        out_puts("\"\n");
    }
}
