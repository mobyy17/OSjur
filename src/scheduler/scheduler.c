#include "header/scheduler/scheduler.h"
#include "header/cpu/interrupt.h"
#include "header/memory/paging.h"

// Slot _process_list yang terakhir dijalankan. Diawali slot terakhir
// supaya pencarian pertama jatuh ke slot 0 (shell).
static uint32_t last_slot = PROCESS_COUNT_MAX - 1;

void scheduler_init(void) {
    activate_timer_interrupt();
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();
    if (pcb != NULL)
        pcb->context = ctx;
}

void scheduler_switch_to_next_process(void) {
    // Process lama kembali ke antrean. NULL kalau process lama baru saja dihapus (exit/kill)
    struct ProcessControlBlock *current = process_get_current_running_pcb_pointer();
    if (current != NULL)
        current->metadata.state = READY;

    // Round robin: cari slot aktif berikutnya secara melingkar, mulai setelah last_slot.
    // i sampai PROCESS_COUNT_MAX supaya slot last_slot sendiri ikut dicek (kasus single process).
    for (uint32_t i = 1; i <= PROCESS_COUNT_MAX; i++) {
        uint32_t slot = (last_slot + i) % PROCESS_COUNT_MAX;
        struct ProcessControlBlock *next = &_process_list[slot];
        if (!next->metadata.active || next->metadata.state != READY)
            continue;

        last_slot = slot;
        next->metadata.state = RUNNING;
        // ctx yang dikirim berada di stack kernel (higher half), tetap valid setelah CR3 diganti
        paging_use_page_directory(next->context.page_directory_virtual_addr);
        process_context_switch(next->context);
    }

    // Tidak terjadi selama shell (PID 0) masih ada, karena shell tidak bisa dihapus
    while (true);
}