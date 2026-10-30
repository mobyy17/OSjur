#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/cpu/gdt.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"

struct TSSEntry _interrupt_tss_entry = {
    .ss0 = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void) {
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile ("mov %%ebp, %0": "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8;
}

void io_wait(void) {
    out(0x80, 0);
}
 
void pic_ack(uint8_t irq) {
    if (irq >= 8) out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}
 
void pic_remap(void) {
    // Start initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: master vector offset (0x20)
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: slave vector offset (0x28)
    io_wait();
    out(PIC1_DATA, 0b0100);      // ICW3: master, slave PIC is at IRQ2
    io_wait();
    out(PIC2_DATA, 0b0010);      // ICW3: slave, cascade identity
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();
 
    // disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}
 
void activate_keyboard_interrupt(void) {
    // Mask PIC1: nolkan bit IRQ1 saja, bit lain dibiarkan apa adanya
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
        case SYSCALL_INT_NUMBER:
            syscall(frame);
            break;
    }
}

void syscall(struct InterruptFrame frame) {
    uint32_t ebx = frame.cpu.general.ebx;
    uint32_t ecx = frame.cpu.general.ecx;
    uint32_t edx = frame.cpu.general.edx;

    switch (frame.cpu.general.eax) {
        case 0: // read(request), ebx = pointer request, ecx = pointer return code
            *((int8_t*) ecx) = read(*(struct EXT2DriverRequest*) ebx);
            break;
        case 1: // read_directory(request)
            *((int8_t*) ecx) = read_directory((struct EXT2DriverRequest*) ebx);
            break;
        case 2: // write(request)
            *((int8_t*) ecx) = write((struct EXT2DriverRequest*) ebx);
            break;
        case 3: // delete(request)
            *((int8_t*) ecx) = delete(*(struct EXT2DriverRequest*) ebx);
            break;
        case 4: // getchar, ebx = pointer char (0 kalau belum ada input)
            get_keyboard_buffer((char*) ebx);
            break;
        case 5: // putchar, ebx = karakter, ecx = warna
            framebuffer_putchar((char) ebx, (uint8_t) ecx);
            break;
        case 6: // puts, ebx = pointer string, ecx = jumlah karakter, edx = warna
            framebuffer_puts((char*) ebx, ecx, (uint8_t) edx);
            break;
        case 7: // aktifkan input keyboard
            keyboard_state_activate();
            break;
        case 8: // move(src, dst), ebx = pointer request asal, ecx = pointer return code, edx = pointer request tujuan
            *((int8_t*) ecx) = move(*(struct EXT2DriverRequest*) ebx, *(struct EXT2DriverRequest*) edx);
            break;
    }
}