#include "header/shell/shell.h"

#define EXEC_BUFFER_SIZE (1024 * 1024)

/* Cari executable: folder sekarang dulu, lalu tiap folder di PATH (default "/", tempat inserter menaruh aplikasi) */
static bool find_program(const char *name, uint32_t *parent) {
    uint32_t inode = 0;
    bool is_dir = false;
    if (dir_lookup(cwd_inode, name, &inode, &is_dir) && !is_dir) {
        *parent = cwd_inode;
        return true;
    }
    return path_find_program(name, parent);
}

/**
 * Jalankan argv[0] sebagai process terpisah dengan argc/argv, lalu shell MENUNGGU sampai process itu selesai (beda dengan exec yang langsung kembali ke prompt).
 * Selama menunggu, Ctrl+C mematikan process tersebut.
 */
bool run_program(int argc, char *argv[]) {
    uint32_t parent = 0;
    if (str_eq(argv[0], "shell") || !find_program(argv[0], &parent))
        return false;

    struct EXT2DriverRequest request = {
        .buf          = (void*) 0,
        .name         = argv[0],
        .name_len     = (uint8_t) str_len(argv[0]),
        .parent_inode = parent,
        .buffer_size  = EXEC_BUFFER_SIZE,
        .is_directory = false,
    };
    struct ExecArgs args = { .argc = (uint32_t) argc, .pid = 0 };
    for (int i = 0; i < argc; i++)
        args.argv[i] = argv[i];

    int32_t retcode = -1;
    syscall(SYSCALL_EXEC_ARGS, (uint32_t) &request, (uint32_t) &retcode, (uint32_t) &args);
    if (retcode != EXEC_SUCCESS) {
        out_puts(argv[0]);
        out_puts(": gagal dijalankan\n");
        return true;
    }

    // Output shell yang tertunda dicetak dulu supaya urutannya benar
    out_flush();
    while (process_alive(args.pid)) {
        char c = 0;
        syscall(SYSCALL_GETCHAR, (uint32_t) &c, 0, 0);
        if (c == KEY_CTRL_C_CHAR) {
            bool killed = false;
            syscall(SYSCALL_KILL, args.pid, (uint32_t) &killed, 0);
            // Program fullscreen (misalnya badapple) meninggalkan sisa gambar, jadi layar dibersihkan
            syscall(SYSCALL_CLEAR_SCREEN, 0, 0, 0);
            print("^C\n", COLOR_TEXT);
            break;
        }
    }
    return true;
}
