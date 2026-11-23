#include <stdint.h>
#include <stdbool.h>
#include "header/shell/shell.h"
#include "header/stdlib/string.h"

/* State shell, dipakai bersama oleh semua command (lihat header/shell/shell.h) */
uint32_t cwd_inode               = ROOT_INODE;
char     cwd_path[SHELL_PATH_MAX] = "/";
char     *pipe_in                = 0;
uint32_t pipe_in_len             = 0;

void shell_print_prompt(void) {
    print("OSjur", COLOR_PROMPT);
    print(":", COLOR_TEXT);
    print(cwd_path, COLOR_PATH);
    print("$ ", COLOR_TEXT);
}

/**
 * Pecah str menjadi argumen berdasarkan spasi. str diubah di tempat:
 * spasi pemisah diganti '\0', argv menunjuk ke awal tiap kata.
 *
 * @return Jumlah argumen (argc)
 */
static int parse_args(char *str, char *argv[]) {
    int argc = 0;
    uint32_t i = 0;
    while (str[i] != '\0') {
        while (str[i] == ' ')
            str[i++] = '\0';
        if (str[i] == '\0')
            break;
        if (argc < SHELL_ARGV_MAX)
            argv[argc++] = &str[i];
        while (str[i] != '\0' && str[i] != ' ')
            i++;
    }
    return argc;
}

static void run_command(int argc, char *argv[]) {
    char *cmd = argv[0];
    if      (str_eq(cmd, "cd"))    cmd_cd(argc, argv);
    else if (str_eq(cmd, "ls"))    cmd_ls(argc, argv);
    else if (str_eq(cmd, "mkdir")) cmd_mkdir(argc, argv);
    else if (str_eq(cmd, "cat"))   cmd_cat(argc, argv);
    else if (str_eq(cmd, "cp"))    cmd_cp(argc, argv);
    else if (str_eq(cmd, "rm"))    cmd_rm(argc, argv);
    else if (str_eq(cmd, "mv"))    cmd_mv(argc, argv);
    else if (str_eq(cmd, "find"))  cmd_find(argc, argv);
    else if (str_eq(cmd, "grep"))  cmd_grep(argc, argv);
    else if (str_eq(cmd, "exec"))  cmd_exec(argc, argv);
    else if (str_eq(cmd, "ps"))    cmd_ps(argc, argv);
    else if (str_eq(cmd, "kill"))  cmd_kill(argc, argv);
    else if (str_eq(cmd, "sh"))       cmd_sh(argc, argv);
    else if (str_eq(cmd, "beep"))     cmd_beep(argc, argv);
    else {
        out_puts(cmd);
        out_puts(": perintah tidak dikenal\n");
    }
}

/**
 * Jalankan satu baris input. Baris dipotong di setiap '|'.
 * Output command di kiri '|' tidak dicetak, tapi disalin ke pipe_buffer
 * dan menjadi pipe_in untuk command di kanannya. Output command terakhir
 * tetap di buffer output dan dicetak oleh main().
 */
/* Buffer milik main(), disimpan supaya shell_run_line bisa dipanggil dari sh */
static char *shell_out_buffer;
static char *shell_pipe_buffer;

static void run_line(char *line, char *out_buffer, char *pipe_buffer) {
    char *segment = line;
    pipe_in     = 0;
    pipe_in_len = 0;

    while (segment != 0) {
        // potong di '|' pertama, sisanya diproses di putaran berikutnya
        char *next = 0;
        for (uint32_t i = 0; segment[i] != '\0'; i++) {
            if (segment[i] == '|') {
                segment[i] = '\0';
                next = &segment[i + 1];
                break;
            }
        }

        char *argv[SHELL_ARGV_MAX];
        int argc = parse_args(segment, argv);
        if (argc == 0) {
            if (next != 0 || pipe_in != 0) { // "ls |" atau "| ls"
                out_reset();
                out_puts("shell: pipe tidak lengkap\n");
            }
            break;
        }
        run_command(argc, argv);

        if (next != 0) {
            memcpy(pipe_buffer, out_buffer, out_len());
            pipe_in     = pipe_buffer;
            pipe_in_len = out_len();
            out_reset();
        }
        segment = next;
    }

    pipe_in     = 0;
    pipe_in_len = 0;
}

void shell_run_line(char *line) {
    run_line(line, shell_out_buffer, shell_pipe_buffer);
}

int main(void) {
    // Buffer besar ditaruh di stack user (sekitar 3 MiB tersedia di bawah 0x400000),
    // bukan global, supaya ukuran file shell di disk tetap kecil.
    char line[SHELL_LINE_MAX];
    char out_buffer[SHELL_OUT_MAX];
    char pipe_buffer[SHELL_OUT_MAX];

    out_init(out_buffer);
    shell_out_buffer  = out_buffer;
    shell_pipe_buffer = pipe_buffer;
    syscall(SYSCALL_KEYBOARD_ON, 0, 0, 0);

    while (true) {
        shell_print_prompt();

        // "OSjur:" + path + "$ " sudah memakai sebagian dari 80 kolom
        uint32_t prompt_len = 6 + str_len(cwd_path) + 2;
        uint32_t max_len    = SHELL_LINE_MAX - 1;
        if (prompt_len + max_len > 79)
            max_len = prompt_len < 79 ? 79 - prompt_len : 0;

        if (!shell_read_line(line, max_len))
            continue;

        run_line(line, out_buffer, pipe_buffer);
        out_flush();
    }
    return 0;
}
