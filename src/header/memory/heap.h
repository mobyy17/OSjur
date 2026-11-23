#ifndef _HEAP_H
#define _HEAP_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Heap per process (bonus). Tiap process sudah punya 4 MiB virtual yang terpetakan dimana program di 0x0 (maks 1 MiB) dan stack turun dari 0x400000. Ruang di tengahnya dipakai jadi heap, jadi tidak perlu frame baru dan paging.c tidak disentuh.
 *
 * Yang mengatur alokasi adalah kernel (kernel_malloc/kernel_free), program user memanggilnya lewat syscall 22 dan 23. Header tiap blok ikut tersimpan di memori process itu sendiri, di depan data yang dikembalikan ke user.
 */
#define HEAP_START 0x100000u  // tepat setelah batas 1 MiB program
#define HEAP_END   0x380000u  // sisakan 512 KiB di bawah 0x400000 untuk stack

/* Ringkasan isi heap process sekarang, dipakai command meminfo (syscall 24) */
struct HeapInfo {
    uint32_t total;
    uint32_t used;
    uint32_t free;
    uint32_t block_count;
    uint32_t used_blocks;
    uint32_t largest_free;
};

/* Satu blok untuk dump daftar blok (syscall 26) */
struct HeapBlockInfo {
    uint32_t addr; // alamat data (yang dikembalikan malloc)
    uint32_t size; // ukuran data tanpa header
    bool     used;
};

/**
 * Alokasi memori di heap process yang sedang jalan. First-fit: blok kosong pertama yang cukup dipakai, sisanya dipotong jadi blok kosong baru kalau masih layak.
 * @return Alamat virtual (8-byte aligned), 0 kalau size 0 atau heap penuh
 */
uint32_t kernel_malloc(uint32_t size);

/**
 * Bebaskan blok, lalu gabungkan blok kosong yang bersebelahan supaya heap tidak terpecah.
 * @return false kalau ptr bukan hasil malloc atau sudah di-free (double free)
 */
bool kernel_free(uint32_t ptr);

void heap_get_info(struct HeapInfo *info);

/** @return false kalau index melewati blok terakhir */
bool heap_get_block(uint32_t index, struct HeapBlockInfo *block);

#endif
