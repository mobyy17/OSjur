#include "header/process/process.h"

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

struct ProcessControlBlock* process_get_current_running_pcb_pointer(void) {
    return NULL;
}

int32_t process_create_user_process(struct EXT2DriverRequest request) {
    (void) request;
    return PROCESS_CREATE_FAIL_FS_READ_FAILURE;
}

bool process_destroy(uint32_t pid) {
    (void) pid;
    return false;
}

bool process_get_info(uint32_t slot, struct ProcessInfo *info) {
    (void) slot;
    (void) info;
    return false;
}
