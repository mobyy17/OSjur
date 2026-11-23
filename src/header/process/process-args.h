#ifndef _PROCESS_ARGS_H
#define _PROCESS_ARGS_H

#include <stdint.h>
#include "header/filesystem/ext2.h"
#include "header/cpu/syscall-ext.h"

/**
 * Buat process baru seperti process_create_user_process(), lalu taruh argc dan argv
 * di puncak user stack-nya sehingga main(int argc, char *argv[]) bisa membacanya.
 *
 * @param request Request executable (pointer di dalamnya milik process pemanggil)
 * @param args    argc dan argv milik process pemanggil, pid diisi kalau berhasil
 * @return        Kode PROCESS_CREATE_*
 */
int32_t process_create_user_process_with_args(struct EXT2DriverRequest request, struct ExecArgs *args);

#endif
