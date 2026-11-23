#include "header/shell/shell.h"

void cmd_shutdown(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    print("Sampai jumpa dimasa yang akan datang pips!\n", COLOR_PROMPT);
    beep(660, 120);
    beep(440, 180);
    syscall(SYSCALL_SHUTDOWN, 0, 0, 0);
}
