#ifndef _PAGING_H
#define _PAGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/** 
 * Note: MB often referring to MiB in context of memory management
 * change this to be a small number to test if swap working properly
 * kernel will be tested with a small number of SYSTEM_MEMORY_MB
 */
 #define SYSTEM_MEMORY_MB     128 

// Number of entries in a page directory
#define PAGE_ENTRY_COUNT     1024

// Page Frame (PF) Size: 4 KiB
#define PAGE_FRAME_SIZE      0x1000u

// Higher half dimana physical 0x0 - 0x400000 dipetakan ke virtual 0xC0000000 - 0xC0400000
#define KERNEL_VIRTUAL_ADDRESS_BASE 0xC0000000u

#define KERNEL_RESERVED_PAGE_FRAME_COUNT (0x400000u / PAGE_FRAME_SIZE)
#define RECURSIVE_PAGE_TABLES_VIRTUAL_ADDRESS 0xFFC00000u
#define RECURSIVE_PAGE_DIRECTORY_INDEX 1023u

// Maximum usable page frame. Default count: 128 MiB / 4 KiB = 32768 frames
#define PAGE_FRAME_MAX_COUNT ((SYSTEM_MEMORY_MB << 20) / PAGE_FRAME_SIZE)

// kernel's  page directory, using 4 KiB pages
extern struct PageDirectory _paging_kernel_page_directory;
extern struct PageTable _paging_kernel_page_table;

/**
 * Page Directory Entry Flag, only first 8 bit
 * @param present_bit        Indicate whether this entry is exist or not.
 * @param write_bit          Indicate whether this page is writable
 * @param us_bit             User/Supervisor, 1 = user mode boleh akses
 * @param pwt_bit            Page-level write-through
 * @param pcd_bit            Page-level cache disable
 * @param accessed_bit       Diset MMU ketika entry ini dipakai translasi
 * @param ignored_bit        Diabaikan CPU untuk PDE yang menunjuk page table
 * @param use_pagesize_4_mb  Indicate whether this entry use 4 MB page size (set false)
 * References: Intel Manual 3a - Ch 4 Paging - Figure 4-4 PDE: 4KB page
 */
struct PageDirectoryEntryFlag {
    uint32_t present_bit        : 1;
    uint32_t write_bit          : 1;
    uint32_t us_bit             : 1;
    uint32_t pwt_bit            : 1;
    uint32_t pcd_bit            : 1;
    uint32_t accessed_bit       : 1;
    uint32_t ignored_bit        : 1;
    uint32_t use_pagesize_4_mb  : 1;
} __attribute__((packed));

/**
 * Page Table Entry Flag, only first 8 bit
 * @param present_bit  Indicate whether this entry is exist or not.
 * @param write_bit    Indicate whether this page is writable
 * @param us_bit       User/Supervisor, 1 = user mode boleh akses
 * @param pwt_bit      Page-level write-through
 * @param pcd_bit      Page-level cache disable
 * @param accessed_bit Diset MMU ketika page ini diakses
 * @param dirty_bit    Diset MMU ketika page ini ditulis
 * @param pat_bit      Page Attribute Table (set false)
 * References: Intel Manual 3a - Ch 4 Paging - Figure 4-4 PTE: 4KB page
 */
struct PageTableEntryFlag {
    uint32_t present_bit        : 1;
    uint32_t write_bit          : 1;
    uint32_t us_bit             : 1;
    uint32_t pwt_bit            : 1;
    uint32_t pcd_bit            : 1;
    uint32_t accessed_bit       : 1;
    uint32_t dirty_bit          : 1;
    uint32_t pat_bit            : 1;
} __attribute__((packed));

/**
 * Page Directory Entry, for page size 4 KB. 
 * References: Intel Manual 3a - Ch 4 Paging - Figure 4-4, PDE nya 4KB page
 *
 * @param flag                    Contain 8-bit page directory entry flag
 * @param global_page             Bit 8, diabaikan CPU untuk PDE 4 KB
 * @param ignored                 Ignored bit 11:9 (3-bit)
 * @param page_table_base_address Bit 31:12, nomor frame physical tempat PageTable berada
 */
struct PageDirectoryEntry {
    struct PageDirectoryEntryFlag flag;
    uint16_t global_page             : 1;
    uint16_t ignored                 : 3;
    uint32_t page_table_base_address : 20;
}__attribute__((packed));

/**
 * Page Table Entry, for page size 4 KB. 
 * References: Intel Manual 3a - Ch 4 Paging - Figure 4-4 PTE: 4KB page
 *
 * @param flag                       Contain 8-bit page table entry flag
 * @param global_page                G flag
 * @param ignored                    Ignored bit 11:9 (3-bit)
 * @param physical_page_base_address Bit 31:12, nomor frame physical yang dipetakan
 */
struct PageTableEntry {
    struct PageTableEntryFlag flag;
    uint16_t global_page                : 1;
    uint16_t ignored                    : 3;
    uint32_t physical_page_base_address : 20;
}__attribute__((packed));

/**
 * Page Directory, contain array of PageDirectoryEntry.
 * Note: This data structure is volatile (can be modified from outside this code, check "C volatile keyword").
 * MMU operation, TLB hit & miss also affecting this data structure (dirty, accessed bit, etc).
 * 
 * Warning: Address must be aligned in 4 KB (listed on Intel Manual), use __attribute__((aligned(0x1000))),
 * unaligned definition of PageDirectory will cause triple fault
 *
 * @param table Fixed-width array of PageDirectoryEntry with size PAGE_ENTRY_COUNT
 */
struct PageDirectory {
    struct PageDirectoryEntry table[PAGE_ENTRY_COUNT];
}__attribute__((packed));

/**
 * Page Table, contain array of PageTableEntry
 * Warning: Address juga harus aligned 4 KB
 *
 * @param table Fixed-width array of PageTableEntry with size PAGE_ENTRY_COUNT
 */
struct PageTable {
    struct PageTableEntry table[PAGE_ENTRY_COUNT];
}__attribute__((packed));

/**
 * Containing page manager states.
 * 
 * @param page_frame_map        Keeping track empty space. True when the page frame is currently used
 * @param free_page_frame_count Jumlah page frame yang masih kosong
 */
struct PageManagerState {
    bool     page_frame_map[PAGE_FRAME_MAX_COUNT];
    uint32_t free_page_frame_count;
} __attribute__((packed));





/**
 * Edit page table entry yang memetakan virtual_addr. Page table dibuat otomatis jika belum ada.
 * Hanya bisa dipakai pada page directory yang sedang aktif (CR3).
 * 
 * @param page_dir      Page directory to update
 * @param physical_addr Physical address to map
 * @param virtual_addr  Virtual address to map
 * @param flag          Page table entry flags
 */
void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr,
    void *virtual_addr,
    struct PageTableEntryFlag flag
);

/**
 * Get the requested page table from respective parameter
 * @param page_dir      Page directory to read
 * @param page_directory_index page table's position/index on page directory 
 * note: sudah diberikan implementasinya pada paging.c dengan pendekatan fractal memory maping alias recursive memory mapping.
 * read: https://wiki.osdev.org/Fractal_Page_Mapping
 * @return Virtual address page table, NULL jika page_dir bukan page directory aktif
 *         atau tidak ada frame kosong untuk page table baru
 */
struct PageTable *paging_get_page_table(struct PageDirectory *page_dir, uint32_t page_directory_index);

/**
 * Ambil virtual address page directory yang sedang aktif (dibaca dari CR3)
 * 
 * @return Pointer (virtual address) page directory aktif
 */
struct PageDirectory* paging_get_current_page_directory_addr(void);

/**
 * Invalidate page that contain virtual address in parameter
 * 
 * @param virtual_addr Virtual address to flush
 */
void flush_single_tlb(void *virtual_addr);





/* --- Memory Management --- */
/**
 * Check whether a certain amount of physical memory is available
 * 
 * @param amount Requested amount of physical memory in bytes
 * @return       Return true when there's enough free memory available
 */
bool paging_allocate_check(uint32_t amount);

/**
 * Allocate single user page frame in page directory
 * 
 * @param page_dir     Page directory to update
 * @param virtual_addr Virtual address to be allocated
 * @return             True jika berhasil, false jika virtual_addr di kernel space,
 *                     sudah terpetakan, atau memory habis
 */
bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr);

/**
 * Deallocate single user page frame in page directory
 * 
 * @param page_dir      Page directory to update
 * @param virtual_addr  Virtual address to be deallocated
 * @return              Will return true if success, false otherwise
 */
bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr);

#endif