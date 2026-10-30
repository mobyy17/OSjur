#include "header/shell/shell.h"

/* cd [folder]: pindah ke folder di folder sekarang.
 * - "cd" tanpa argumen atau "cd /" kembali ke root
 * - "cd .." naik satu folder (tetap di root kalau sudah di root)
 * Path bertingkat (cd a/b) tidak didukung, sesuai spek (relative path tidak wajib). */
void cmd_cd(int argc, char *argv[]) {
    if (argc > 2) {
        out_puts("pemakaian: cd [folder]\n");
        return;
    }

    if (argc == 1 || str_eq(argv[1], "/")) {
        cwd_inode   = ROOT_INODE;
        cwd_path[0] = '/';
        cwd_path[1] = '\0';
        return;
    }

    char *name = argv[1];
    if (str_eq(name, "."))
        return;

    uint32_t inode;
    bool is_dir;
    if (!dir_lookup(cwd_inode, name, &inode, &is_dir)) {
        out_puts("cd: ");
        out_puts(name);
        out_puts(" tidak ditemukan\n");
        return;
    }
    if (!is_dir) {
        out_puts("cd: ");
        out_puts(name);
        out_puts(" bukan folder\n");
        return;
    }

    if (str_eq(name, "..")) {
        // buang komponen terakhir: "/a/b" -> "/a", "/a" -> "/"
        uint32_t len = str_len(cwd_path);
        while (len > 1 && cwd_path[len - 1] != '/')
            len--;
        if (len > 1)
            len--; // buang '/' pemisah, kecuali '/' milik root
        cwd_path[len] = '\0';
    } else {
        // tambah "/nama", "/" pemisah tidak ditambah kalau masih di root
        uint32_t len      = str_len(cwd_path);
        uint32_t name_len = str_len(name);
        uint32_t sep      = (len > 1) ? 1 : 0;
        if (len + sep + name_len >= SHELL_PATH_MAX) {
            out_puts("cd: path terlalu panjang\n");
            return;
        }
        if (sep)
            cwd_path[len++] = '/';
        for (uint32_t i = 0; i < name_len; i++)
            cwd_path[len++] = name[i];
        cwd_path[len] = '\0';
    }
    cwd_inode = inode;
}