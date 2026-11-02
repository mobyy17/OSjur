#include "header/scheduler/scheduler.h"

void scheduler_init(void) {
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    (void) ctx;
}

void scheduler_switch_to_next_process(void) {
    while (true);
}
