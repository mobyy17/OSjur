#include "header/shell/shell.h"

static const char *command_names[] = {
    "cd", "ls", "mkdir", "cat", "cp", "rm", "mv", "find", "grep", "exec", "ps", "kill", "sh", "clear", "shutdown", "beep", "help", "osjur", "fetch", "bg", "meminfo", "export", "path",
};
#define COMMAND_COUNT (sizeof(command_names) / sizeof(command_names[0]))

#define CANDIDATE_MAX 64

/* Kandidat dikumpulkan ke sini. Nama file EXT2 tidak null-terminated, jadi disalin dulu. */
static char     candidates[CANDIDATE_MAX][SHELL_LINE_MAX];
static bool     candidate_is_dir[CANDIDATE_MAX];
static uint32_t candidate_count;

static bool starts_with(const char *s, uint32_t s_len, const char *prefix, uint32_t prefix_len) {
    if (s_len < prefix_len)
        return false;
    for (uint32_t i = 0; i < prefix_len; i++)
        if (s[i] != prefix[i])
            return false;
    return true;
}

static void add_candidate(const char *name, uint32_t name_len, bool is_dir) {
    if (candidate_count >= CANDIDATE_MAX || name_len >= SHELL_LINE_MAX)
        return;
    for (uint32_t i = 0; i < candidate_count; i++) // hindari duplikat (misal file "ls" dan command "ls")
        if (str_len(candidates[i]) == name_len && starts_with(candidates[i], name_len, name, name_len))
            return;
    for (uint32_t i = 0; i < name_len; i++)
        candidates[candidate_count][i] = name[i];
    candidates[candidate_count][name_len] = '\0';
    candidate_is_dir[candidate_count] = is_dir;
    candidate_count++;
}

static void collect_files(const char *prefix, uint32_t prefix_len) {
    uint8_t table[SHELL_DIR_MAX];
    uint32_t size = dir_read_table(cwd_inode, table);
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry;
    while ((entry = dir_next_entry(table, size, &offset)) != 0) {
        char *name = entry_name(entry);
        if (name[0] == '.' && prefix_len == 0)
            continue; // "." dan ".." tidak ditawarkan kecuali user mulai mengetik titik
        if (starts_with(name, entry->name_len, prefix, prefix_len))
            add_candidate(name, entry->name_len, entry->file_type == EXT2_FT_DIR);
    }
}

uint32_t autocomplete(char *line, uint32_t len, uint32_t max_len) {
    // Kata yang sedang diketik = mulai dari spasi terakhir sampai ujung
    uint32_t word_start = len;
    while (word_start > 0 && line[word_start - 1] != ' ' && line[word_start - 1] != '|')
        word_start--;
    char *word = &line[word_start];
    uint32_t word_len = len - word_start;

    // Kata pertama (atau kata pertama setelah '|') = nama command
    uint32_t before = word_start;
    while (before > 0 && line[before - 1] == ' ')
        before--;
    bool is_command = before == 0 || line[before - 1] == '|';

    candidate_count = 0;
    if (is_command)
        for (uint32_t i = 0; i < COMMAND_COUNT; i++)
            if (starts_with(command_names[i], str_len(command_names[i]), word, word_len))
                add_candidate(command_names[i], str_len(command_names[i]), false);
    collect_files(word, word_len); // file di folder sekarang juga bisa dijalankan langsung

    if (candidate_count == 0) {
        beep(880, 40);
        return len;
    }

    // Panjang awalan yang sama di semua kandidat
    uint32_t common = str_len(candidates[0]);
    for (uint32_t i = 1; i < candidate_count; i++) {
        uint32_t j = 0;
        while (j < common && candidates[i][j] == candidates[0][j])
            j++;
        common = j;
    }

    // Lengkapi sampai awalan yang sama
    uint32_t new_len = len;
    for (uint32_t i = word_len; i < common && new_len < max_len; i++)
        line[new_len++] = candidates[0][i];

    if (candidate_count == 1) {
        if (!candidate_is_dir[0] && new_len < max_len)
            line[new_len++] = ' '; // nama lengkap, langsung siap untuk argumen berikutnya
        return new_len;
    }

    // Masih banyak pilihan dan tidak ada yang bisa ditambahkan: tampilkan semuanya
    if (new_len == len) {
        print("\n", COLOR_TEXT);
        for (uint32_t i = 0; i < candidate_count; i++) {
            print(candidates[i], candidate_is_dir[i] ? COLOR_PATH : COLOR_TEXT);
            print("  ", COLOR_TEXT);
        }
        print("\n", COLOR_TEXT);
        shell_print_prompt();
        line[len] = '\0';
        print(line, COLOR_TEXT);
    }
    return new_len;
}
