#include "header/shell/shell.h"

/* Easter egg kecil: tampilkan lagi banner pembuka */
void cmd_osjur(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    syscall(SYSCALL_CLEAR_SCREEN, 0, 0, 0);
    banner_show(true);
}
