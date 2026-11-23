#include "header/shell/shell.h"

void cmd_help(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    out_puts("File  : ls cd mkdir cat cp rm mv find grep\n");
    out_puts("Proses: exec <file> [arg] (background), ps, kill <pid>\n");
    out_puts("        <nama program> [arg] (foreground, Ctrl+C untuk stop)\n");
    out_puts("Lain  : sh <file.sh>, clear, beep [hz] [ms], shutdown, fetch, bg <warna>\n");
    out_puts("Memori: meminfo [-v], export PATH=/a:/b, path (dump array PATH)\n");
    out_puts("Tombol: panah atas/bawah = riwayat, Tab = autocomplete, PgUp/PgDn = scroll\n");
}
