#include "header/shell/shell.h"

/* Ubah string desimal jadi angka. false kalau bukan angka atau terlalu besar. */
static bool parse_uint(const char *s, uint32_t *value) {
    if (*s == '\0')
        return false;
    uint32_t result = 0;
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9')
            return false;
        uint32_t digit = (uint32_t) (*s - '0');
        if (result > (0xFFFFFFFFu - digit) / 10)
            return false;
        result = result * 10 + digit;
    }
    *value = result;
    return true;
}

/* kill <pid>: hentikan process. PID 0 (shell) ditolak kernel. */
void cmd_kill(int argc, char *argv[]) {
    if (argc != 2) {
        out_puts("pemakaian: kill <pid>\n");
        return;
    }

    uint32_t pid = 0;
    if (!parse_uint(argv[1], &pid)) {
        out_puts("kill: ");
        out_puts(argv[1]);
        out_puts(" bukan PID yang valid\n");
        return;
    }
    if (pid == 0) {
        out_puts("kill: shell (PID 0) tidak bisa di-kill\n");
        return;
    }

    bool success = false;
    syscall(SYSCALL_KILL, pid, (uint32_t) &success, 0);
    if (!success) {
        out_puts("kill: tidak ada process dengan PID ");
        out_puts(argv[1]);
        out_putchar('\n');
    }
}
