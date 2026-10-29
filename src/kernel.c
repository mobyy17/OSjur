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

    // Allocate first 4 MiB virtual memory.
    // Kit kita memakai page 4 KiB, jadi perlu 1024 kali alokasi: shell di 0x0 dan
    // user stack (diset kernel_execute_user_program) di 0x400000 - 4 dua-duanya harus terpetakan.
    for (uint32_t addr = 0; addr < 0x400000; addr += PAGE_FRAME_SIZE)
        paging_allocate_user_page_frame(&_paging_kernel_page_directory, (void*) addr);

    // Write shell into memory
    struct EXT2DriverRequest request = {
        .buf          = (uint8_t*) 0,
        .name         = "shell",
        .name_len     = 5,
        .parent_inode = 1, // root directory
        .buffer_size  = 0x100000,
    };
    if (read(request) != 0) {
        framebuffer_puts("Gagal membaca shell, jalankan make insert-shell", 47, 0xC);
        while (true);
    }

    // Set TSS $esp pointer and jump into shell
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t*) 0);

    while (true);
}