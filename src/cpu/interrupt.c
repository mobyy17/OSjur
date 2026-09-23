#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"

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
    }
}