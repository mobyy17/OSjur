#include "header/memory/heap.h"
#include "header/process/process.h"

#define HEAP_ALIGN      8
#define HEAP_MAGIC_USED 0xA110C8EDu
#define HEAP_MAGIC_FREE 0xF4EEB10Cu

/* Header 8 byte di depan setiap blok. size = ukuran blok termasuk header. Blok tersusun berurutan dari HEAP_START sampai HEAP_END, jadi blok berikutnya selalu di (alamat blok + size). */
struct HeapBlock {
    uint32_t size;
    uint32_t magic;
};

#define HEADER_SIZE     ((uint32_t) sizeof(struct HeapBlock))
#define MIN_SPLIT_SIZE  (HEADER_SIZE + HEAP_ALIGN)

/* Heap tiap slot process diinisialisasi saat pertama dipakai. Kalau slot sudah dipakai process lain (PID beda), heap nya dianggap baru lagi. */
static bool     heap_ready[PROCESS_COUNT_MAX];
static uint32_t heap_owner[PROCESS_COUNT_MAX];

static bool heap_prepare(void) {
    struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();
    if (pcb == NULL)
        return false;
    uint32_t slot = (uint32_t) (pcb - _process_list);
    if (!heap_ready[slot] || heap_owner[slot] != pcb->metadata.pid) {
        struct HeapBlock *first = (struct HeapBlock*) HEAP_START;
        first->size  = HEAP_END - HEAP_START;
        first->magic = HEAP_MAGIC_FREE;
        heap_ready[slot] = true;
        heap_owner[slot] = pcb->metadata.pid;
    }
    return true;
}

static bool block_valid(struct HeapBlock *b) {
    uint32_t addr = (uint32_t) b;
    return b->size >= HEADER_SIZE && b->size % HEAP_ALIGN == 0 && b->size <= HEAP_END - addr
        && (b->magic == HEAP_MAGIC_USED || b->magic == HEAP_MAGIC_FREE);
}

/* Blok berikutnya, atau NULL kalau sudah di ujung atau header nya rusak (header ada di memori user, jadi bisa saja tertimpa program yang salah tulis) */
static struct HeapBlock *next_block(struct HeapBlock *b) {
    if (!block_valid(b))
        return NULL;
    uint32_t addr = (uint32_t) b + b->size;
    return addr < HEAP_END ? (struct HeapBlock*) addr : NULL;
}

uint32_t kernel_malloc(uint32_t size) {
    if (size == 0 || size > HEAP_END - HEAP_START || !heap_prepare())
        return 0;
    uint32_t need = HEADER_SIZE + ((size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1));

    for (struct HeapBlock *b = (struct HeapBlock*) HEAP_START; b != NULL; b = next_block(b)) {
        if (b->magic != HEAP_MAGIC_FREE || b->size < need)
            continue;
        if (b->size - need >= MIN_SPLIT_SIZE) {
            struct HeapBlock *rest = (struct HeapBlock*) ((uint32_t) b + need);
            rest->size  = b->size - need;
            rest->magic = HEAP_MAGIC_FREE;
            b->size     = need;
        }
        b->magic = HEAP_MAGIC_USED;
        return (uint32_t) b + HEADER_SIZE;
    }
    return 0;
}

bool kernel_free(uint32_t ptr) {
    if (ptr < HEAP_START + HEADER_SIZE || ptr >= HEAP_END || ptr % HEAP_ALIGN != 0 || !heap_prepare())
        return false;

    // Cari bloknya dengan menelusuri dari awal, jadi pointer ngawur (misal di tengah blok) ditolak
    struct HeapBlock *target = (struct HeapBlock*) (ptr - HEADER_SIZE);
    struct HeapBlock *b = (struct HeapBlock*) HEAP_START;
    while (b != NULL && b != target)
        b = next_block(b);
    if (b != target || b->magic != HEAP_MAGIC_USED)
        return false;
    b->magic = HEAP_MAGIC_FREE;

    // Coalescing dimana gabungkan semua pasangan blok kosong yang bersebelahan
    b = (struct HeapBlock*) HEAP_START;
    while (b != NULL) {
        struct HeapBlock *n = next_block(b);
        if (n != NULL && block_valid(n) && b->magic == HEAP_MAGIC_FREE && n->magic == HEAP_MAGIC_FREE) {
            b->size += n->size; // n hilang, b dicek lagi dengan tetangga barunya
            continue;
        }
        b = n;
    }
    return true;
}

void heap_get_info(struct HeapInfo *info) {
    info->total = HEAP_END - HEAP_START;
    info->used = info->free = info->block_count = info->used_blocks = info->largest_free = 0;
    if (!heap_prepare())
        return;
    for (struct HeapBlock *b = (struct HeapBlock*) HEAP_START; b != NULL; b = next_block(b)) {
        info->block_count++;
        if (b->magic == HEAP_MAGIC_USED) {
            info->used += b->size;
            info->used_blocks++;
        } else {
            info->free += b->size;
            if (b->size - HEADER_SIZE > info->largest_free)
                info->largest_free = b->size - HEADER_SIZE;
        }
    }
}

bool heap_get_block(uint32_t index, struct HeapBlockInfo *block) {
    if (!heap_prepare())
        return false;
    uint32_t i = 0;
    for (struct HeapBlock *b = (struct HeapBlock*) HEAP_START; b != NULL; b = next_block(b), i++) {
        if (i == index) {
            block->addr = (uint32_t) b + HEADER_SIZE;
            block->size = b->size - HEADER_SIZE;
            block->used = b->magic == HEAP_MAGIC_USED;
            return true;
        }
    }
    return false;
}
