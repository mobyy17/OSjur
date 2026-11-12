#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/cpu/gdt.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/driver/cmos.h"
#include "header/memory/paging.h"
#include "header/process/process.h"
#include "header/scheduler/scheduler.h"

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

void activate_timer_interrupt(void) {
    __asm__ volatile("cli");
    // Setup how often PIT fire
    uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) (pit_timer_counter_to_fire & 0xFF));
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) ((pit_timer_counter_to_fire >> 8) & 0xFF));

    // Activate the interrupt
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
}

/**
 * IRQ0: simpan context process yang sedang jalan, lalu pindah ke process berikutnya.
 * Timer selalu datang dari ring 3 (kernel berjalan dengan IF = 0), jadi frame.cpu.stack.esp
 * sudah berisi esp milik user (disalin oleh intsetup.s). Fungsi ini tidak kembali ke intsetup.s.
 */
static void timer_isr(struct InterruptFrame frame) {
    struct Context ctx = {
        .cpu                         = frame.cpu,
        .eip                         = frame.int_stack.eip,
        .eflags                      = frame.int_stack.eflags,
        .page_directory_virtual_addr = paging_get_current_page_directory_addr(),
    };
    scheduler_save_context_to_current_running_pcb(ctx);

    // ACK wajib sebelum switch, kalau tidak IRQ0 berikutnya tidak akan pernah datang
    pic_ack(IRQ_TIMER);
    scheduler_switch_to_next_process();
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case PIC1_OFFSET + IRQ_TIMER:
            timer_isr(frame);
            break;
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
        case SYSCALL_INT_NUMBER:
            syscall(frame);
            break;
    }
}

/**
 * Hapus process yang sedang berjalan lalu pindah ke process berikutnya.
 * Process pemanggil sudah tidak ada, jadi tidak boleh return ke sana.
 * PID 0 (shell) ditolak process_destroy(), untuk kasus itu fungsi ini kembali seperti biasa.
 */
static void exit_current_process(void) {
    struct ProcessControlBlock *current = process_get_current_running_pcb_pointer();
    if (current != NULL && process_destroy(current->metadata.pid))
        scheduler_switch_to_next_process();
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
        case 9: // get time, ebx = pointer struct ClockTime
            cmos_read_time((struct ClockTime*) ebx);
            break;
        case 10: // exit, ebx = return code main (belum dipakai)
            exit_current_process();
            break;
        case 11: // exec(request), ebx = pointer request, ecx = pointer return code (int32_t)
            *((int32_t*) ecx) = process_create_user_process(*(struct EXT2DriverRequest*) ebx);
            break;
        case 12: { // kill, ebx = PID, ecx = pointer bool hasil
            struct ProcessControlBlock *current = process_get_current_running_pcb_pointer();
            if (current != NULL && current->metadata.pid == ebx)
                exit_current_process(); // kill diri sendiri sama dengan exit
            *((bool*) ecx) = process_destroy(ebx);
            break;
        }
        case 13: // ps, ebx = index slot, ecx = pointer struct ProcessInfo, edx = pointer bool (false kalau slot kosong)
            *((bool*) edx) = process_get_info(ebx, (struct ProcessInfo*) ecx);
            break;
        case 14: { // tulis di posisi, ebx = row, ecx = col, edx = pointer string (null-terminated), esi = warna fg
            if (ebx >= FRAMEBUFFER_HEIGHT)
                break;
            char *str = (char*) edx;
            for (uint32_t col = ecx; *str != '\0' && col < FRAMEBUFFER_WIDTH; col++, str++)
                framebuffer_write(ebx, col, *str, frame.cpu.index.esi & 0xF, 0x0);
            break;
        }
    }
}