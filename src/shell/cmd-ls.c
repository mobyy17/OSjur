#include "header/shell/shell.h"

/* ls [folder]: tulis isi folder sekarang, atau isi folder di folder sekarang.
 * Satu nama per baris supaya mudah diproses grep lewat pipe. Folder diberi akhiran '/'. */
void cmd_ls(int argc, char *argv[]) {
    if (argc > 2) {
        out_puts("pemakaian: ls [folder]\n");
        return;
    }

    uint32_t dir = cwd_inode;
    if (argc == 2) {
        bool is_dir;
        if (!dir_lookup(cwd_inode, argv[1], &dir, &is_dir)) {
            out_puts("ls: ");
            out_puts(argv[1]);
            out_puts(" tidak ditemukan\n");
            return;
        }
        if (!is_dir) { // seperti UNIX, ls pada file cukup menulis namanya
            out_puts(argv[1]);
            out_putchar('\n');
            return;
        }
    }

    uint8_t table[SHELL_DIR_MAX];
    uint32_t size = dir_read_table(dir, table);
    if (size == 0) {
        out_puts("ls: gagal membaca folder\n");
        return;
    }

    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry;
    while ((entry = dir_next_entry(table, size, &offset)) != 0) {
        char *name = entry_name(entry);
        bool is_dot    = entry->name_len == 1 && name[0] == '.';
        bool is_dotdot = entry->name_len == 2 && name[0] == '.' && name[1] == '.';
        if (is_dot || is_dotdot)
            continue;

        out_write(name, entry->name_len);
        if (entry->file_type == EXT2_FT_DIR)
            out_putchar('/');
        out_putchar('\n');
    }
}
