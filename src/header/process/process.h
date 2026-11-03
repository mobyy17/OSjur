#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "header/cpu/interrupt.h"
#include "header/memory/paging.h"
#include "header/filesystem/ext2.h"

#define PROCESS_NAME_LENGTH_MAX          32
#define PROCESS_COUNT_MAX                16
#define PROCESS_PAGE_FRAME_COUNT_MAX     1024

#define CPU_EFLAGS_BASE_FLAG               0x2
#define CPU_EFLAGS_FLAG_CARRY              0x1
#define CPU_EFLAGS_FLAG_PARITY             0x4
#define CPU_EFLAGS_FLAG_AUX_CARRY          0x10
#define CPU_EFLAGS_FLAG_ZERO               0x40
#define CPU_EFLAGS_FLAG_SIGN               0x80
#define CPU_EFLAGS_FLAG_TRAP               0x100
#define CPU_EFLAGS_FLAG_INTERRUPT_ENABLE   0x200
#define CPU_EFLAGS_FLAG_DIRECTION          0x400
#define CPU_EFLAGS_FLAG_OVERFLOW           0x800
#define CPU_EFLAGS_FLAG_IO_PRIVILEGE       0x3000
#define CPU_EFLAGS_FLAG_NESTED_TASK        0x4000
#define CPU_EFLAGS_FLAG_MODE               0x8000
#define CPU_EFLAGS_FLAG_RESUME             0x10000
#define CPU_EFLAGS_FLAG_VIRTUAL_8086       0x20000
#define CPU_EFLAGS_FLAG_ALIGNMENT_CHECK    0x40000
#define CPU_EFLAGS_FLAG_VINTERRUPT_FLAG    0x80000
#define CPU_EFLAGS_FLAG_VINTERRUPT_PENDING 0x100000
#define CPU_EFLAGS_FLAG_CPUID_INSTRUCTION  0x200000
#define CPU_EFLAGS_FLAG_AES_SCHEDULE_LOAD  0x40000000
#define CPU_EFLAGS_FLAG_ALTER_INSTRUCTION  0x80000000

// Return code constant for process_create_user_process()
#define PROCESS_CREATE_SUCCESS                   0
#define PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED 1
#define PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT   2
#define PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY    3
#define PROCESS_CREATE_FAIL_FS_READ_FAILURE      4

/**
 * State eksekusi process. Slot _process_list yang kosong ditandai metadata.active = false
 */
typedef enum PROCESS_STATE {
    READY,
    RUNNING,
    WAITING,
} PROCESS_STATE;

/**
 * Context CPU sebuah process, disimpan saat context saving dan dipakai saat context switch
 *
 * @param cpu                         Semua register CPU (index, stack, general, segment)
 * @param eip                         Alamat instruksi berikutnya
 * @param eflags                      Register flag
 * @param page_directory_virtual_addr Page directory (virtual address) milik process
 */
struct Context {
    struct CPURegister    cpu;
    uint32_t              eip;
    uint32_t              eflags;
    struct PageDirectory *page_directory_virtual_addr;
};

/**
 * Structure data containing information about a process
 *
 * @param metadata Process metadata, contain various information about process
 * @param context  Process context used for context saving & switching
 * @param memory   Memory used for the process
 */
struct ProcessControlBlock {
    struct {
        bool          active;
        uint32_t      pid;
        char          name[PROCESS_NAME_LENGTH_MAX];
        PROCESS_STATE state;
    } metadata;

    struct Context context;

    struct {
        void     *virtual_addr_used[PROCESS_PAGE_FRAME_COUNT_MAX];
        uint32_t page_frame_used_count;
    } memory;
};

/**
 * Info satu process untuk syscall ps (disalin ke memori user, jadi tanpa pointer)
 *
 * @param pid   Process ID
 * @param name  Nama process (null-terminated)
 * @param state State process
 */
struct ProcessInfo {
    uint32_t      pid;
    char          name[PROCESS_NAME_LENGTH_MAX];
    PROCESS_STATE state;
};

// Daftar semua PCB, index = slot. Didefinisikan di process.c
extern struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];


/**
 * Get currently running process PCB pointer
 *
 * @return Will return NULL if there's no running process
 */
struct ProcessControlBlock* process_get_current_running_pcb_pointer(void);

/**
 * Create new user process and setup the virtual address space.
 * All available return code is defined with macro "PROCESS_CREATE_*"
 *
 * @note          This procedure assumes no reentrancy in ISR
 * @param request Appropriate read request for the executable
 * @return        Process creation return code
 */
int32_t process_create_user_process(struct EXT2DriverRequest request);

/**
 * Destroy process then release page directory and process control block
 *
 * @param pid Process ID to delete
 * @return    True if process destruction success
 */
bool process_destroy(uint32_t pid);

/**
 * Ambil info process pada slot tertentu (dipakai syscall ps)
 *
 * @param slot Index _process_list, 0 sampai PROCESS_COUNT_MAX - 1
 * @param info Tempat menyimpan hasil
 * @return     True jika slot berisi process aktif
 */
bool process_get_info(uint32_t slot, struct ProcessInfo *info);

#endif
