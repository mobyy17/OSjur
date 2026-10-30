#include "header/shell/shell.h"

/* find <nama>: cari file atau folder bernama <nama> di seluruh file system, mulai dari root.
 * Setiap yang ketemu ditulis path lengkapnya, satu per baris (contoh: /folder1/xi). */

/* Telusuri isi folder dir_inode secara rekursif.
 * path berisi path folder ini sepanjang path_len karakter (root = string kosong), lalu path tiap anak ditulis di belakangnya sementara. */
static void find_in(uint32_t dir_inode, char *path, uint32_t path_len, const char *name) {
    uint8_t table[SHELL_DIR_MAX];
    uint32_t size   = dir_read_table(dir_inode, table);
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry;

    while ((entry = dir_next_entry(table, size, &offset)) != 0) {
        char *child = entry_name(entry);

        // "." dan ".." dilewati, kalau tidak penelusuran berputar tanpa henti
        bool is_dot    = entry->name_len == 1 && child[0] == '.';
        bool is_dotdot = entry->name_len == 2 && child[0] == '.' && child[1] == '.';
        if (is_dot || is_dotdot) continue;

        // path anak = path folder + "/" + nama anak
        uint32_t child_len = path_len + 1 + entry->name_len;
        if (child_len >= SHELL_PATH_MAX) continue;
        path[path_len] = '/';
        for (uint32_t i = 0; i < entry->name_len; i++) path[path_len + 1 + i] = child[i];
        path[child_len] = '\0';

        if (str_eq(path + path_len + 1, name)) {
            out_puts(path);
            out_putchar('\n');
        }

        if (entry->file_type == EXT2_FT_DIR) find_in(entry->inode, path, child_len, name);
    }
    path[path_len] = '\0';
}

void cmd_find(int argc, char *argv[]) {
    if (argc != 2) {
        out_puts("pemakaian: find <nama>\n");
        return;
    }

    char path[SHELL_PATH_MAX];
    path[0] = '\0';
    uint32_t before = out_len();
    find_in(ROOT_INODE, path, 0, argv[1]);

    if (out_len() == before) {
        out_puts("find: ");
        out_puts(argv[1]);
        out_puts(" tidak ditemukan\n");
    }
}