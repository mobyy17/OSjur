#include "header/shell/shell.h"

void cmd_clear(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    syscall(SYSCALL_CLEAR_SCREEN, 0, 0, 0);
}
