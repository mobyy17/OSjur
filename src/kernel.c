#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/memory/paging.h"
#include "header/process/process.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_ext2();
    gdt_install_tss();
    set_tss_register();

    // Shell request. Alokasi 4 MiB pertama sekarang dilakukan process_create_user_process() di page directory milik process, bukan lagi di page directory kernel.
    struct EXT2DriverRequest request = {
        .buf          = (uint8_t*) 0,
        .name         = "shell",
        .name_len     = 5,
        .parent_inode = 1, // root directory
        .buffer_size  = 0x100000,
    };

    // Set TSS.esp0 for interprivilege interrupt
    set_tss_kernel_current_stack();

    // Create & execute process 0 (shell sebagai init process)
    if (process_create_user_process(request) != PROCESS_CREATE_SUCCESS) {
        framebuffer_puts("Gagal membuat process shell, jalankan make insert-shell", 55, 0xC);
        while (true);
    }
    paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
    kernel_execute_user_program((uint8_t*) 0);

    while (true);
}
