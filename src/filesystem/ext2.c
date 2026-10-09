#include <stdint.h>
#include <stdbool.h>
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"

/*
 * Pembagian section (Ch.2):
 *   1. Lewi    : fondasi + layer inode
 *   2. Rafi    : layer block
 *   3. Rapip   : layer directory + read
 *   4. Diandra : write + delete
 * Aturan: cuma ubah body fungsi di section sendiri. Kalau butuh fungsi bantu,
 * buat static dan taruh di section sendiri juga.
 */

const uint8_t fs_signature[BLOCK_SIZE] = {
    'O', 'S', 'j', 'u', 'r', ' ', '-', ' ', 'I', 'F', '2', '1', '3', '0', ' ',  ' ',
    'L', 'e', 'w', 'i', ' ', 'R', 'a', 'f', 'i', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'R', 'a', 'p', 'i', 'p', ' ', 'D', 'i', 'a', 'n', 'd', 'r', 'a', ' ', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '6', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

/* State file system di RAM, di-load dari block 1 dan 2 */
static struct EXT2Superblock superblock;
static struct EXT2BlockGroupDescriptorTable bgd_table;


/* ============================================================================
 * SECTION 1 - LEWI: fondasi + layer inode
 * ========================================================================== */

#define INODES_COUNT      (INODES_PER_GROUP * GROUPS_COUNT)
#define META_BLOCK_COUNT  (2 + INODES_TABLE_BLOCK_COUNT)  // block bitmap + inode bitmap + inode table

/* Block pertama metadata (block bitmap) milik group bgd.
 * Group 0 geser 3 block karena block 0-2 dipakai signature, superblock, BGD table. */
static uint32_t group_meta_start(uint32_t bgd) {
    if (bgd == 0)
        return 3;
    return bgd * BLOCKS_PER_GROUP;
}

/* Lokasi block di disk yang menyimpan inode tersebut */
static uint32_t inode_disk_block(uint32_t inode) {
    uint32_t bgd   = inode_to_bgd(inode);
    uint32_t local = inode_to_local(inode);
    return bgd_table.table[bgd].bg_inode_table + local / INODES_PER_TABLE;
}

/* Offset byte inode tersebut di dalam block-nya */
static uint32_t inode_disk_offset(uint32_t inode) {
    return (inode_to_local(inode) % INODES_PER_TABLE) * INODE_SIZE;
}

uint32_t inode_to_bgd(uint32_t inode) {
    return (inode - 1) / INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode) {
    return (inode - 1) % INODES_PER_GROUP;
}

void read_inode(uint32_t inode, struct EXT2Inode *out) {
    struct BlockBuffer b;
    read_blocks(&b, inode_disk_block(inode), 1);
    memcpy(out, b.buf + inode_disk_offset(inode), INODE_SIZE);
}

void sync_node(struct EXT2Inode *node, uint32_t inode) {
    // inode cuma 70 byte, jadi baca 1 block, ubah bagiannya, tulis balik
    struct BlockBuffer b;
    uint32_t block = inode_disk_block(inode);
    read_blocks(&b, block, 1);
    memcpy(b.buf + inode_disk_offset(inode), node, INODE_SIZE);
    write_blocks(&b, block, 1);
}

bool is_inode_used(uint32_t inode) {
    if (inode == 0 || inode > INODES_COUNT)
        return false;

    struct BlockBuffer bitmap;
    uint32_t local = inode_to_local(inode);
    read_blocks(&bitmap, bgd_table.table[inode_to_bgd(inode)].bg_inode_bitmap, 1);
    return (bitmap.buf[local / 8] & (1 << (local % 8))) != 0;
}

bool is_directory_inode(uint32_t inode) {
    if (!is_inode_used(inode))
        return false;

    struct EXT2Inode node;
    read_inode(inode, &node);
    return (node.i_mode & EXT2_S_IFDIR) != 0;
}

void commit_metadata(void) {
    struct BlockBuffer b;

    memset(&b, 0, BLOCK_SIZE);
    memcpy(b.buf, &superblock, sizeof(superblock));
    write_blocks(&b, 1, 1);

    memset(&b, 0, BLOCK_SIZE);
    memcpy(b.buf, &bgd_table, sizeof(bgd_table));
    write_blocks(&b, 2, 1);
}

uint32_t allocate_node(void) {
    struct BlockBuffer bitmap;

    for (uint32_t bgd = 0; bgd < GROUPS_COUNT; bgd++) {
        if (bgd_table.table[bgd].bg_free_inodes_count == 0)
            continue;

        uint32_t bitmap_block = bgd_table.table[bgd].bg_inode_bitmap;
        read_blocks(&bitmap, bitmap_block, 1);

        for (uint32_t local = 0; local < INODES_PER_GROUP; local++) {
            if (bitmap.buf[local / 8] & (1 << (local % 8)))
                continue;

            bitmap.buf[local / 8] |= (1 << (local % 8));
            write_blocks(&bitmap, bitmap_block, 1);

            // free count cuma diubah di RAM, disimpan saat commit_metadata()
            bgd_table.table[bgd].bg_free_inodes_count--;
            superblock.s_free_inodes_count--;

            return bgd * INODES_PER_GROUP + local + 1;
        }
    }
    return 0; // inode habis
}

void deallocate_node(uint32_t inode) {
    if (!is_inode_used(inode))
        return;

    struct EXT2Inode node;
    read_inode(inode, &node);

    // bebaskan semua block data (+ block indirect) milik inode ini
    deallocate_blocks(node.i_block, node.i_blocks);

    // kosongkan isi inode di inode table
    memset(&node, 0, INODE_SIZE);
    sync_node(&node, inode);

    // matikan bit di inode bitmap
    struct BlockBuffer bitmap;
    uint32_t bgd   = inode_to_bgd(inode);
    uint32_t local = inode_to_local(inode);
    read_blocks(&bitmap, bgd_table.table[bgd].bg_inode_bitmap, 1);
    bitmap.buf[local / 8] &= ~(1 << (local % 8));
    write_blocks(&bitmap, bgd_table.table[bgd].bg_inode_bitmap, 1);

    bgd_table.table[bgd].bg_free_inodes_count++;
    superblock.s_free_inodes_count++;
}

bool is_empty_storage(void) {
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector, BOOT_SECTOR, 1);
    return memcmp(boot_sector.buf, fs_signature, BLOCK_SIZE) != 0;
}

void create_ext2(void) {
    struct BlockBuffer b;

    // block 0: signature
    write_blocks(fs_signature, BOOT_SECTOR, 1);

    // superblock, free block dihitung sambil isi BGD di bawah
    memset(&superblock, 0, sizeof(superblock));
    superblock.s_inodes_count      = INODES_COUNT;
    superblock.s_blocks_count      = DISK_SPACE / BLOCK_SIZE;
    superblock.s_r_blocks_count    = 0;
    superblock.s_free_blocks_count = 0;
    superblock.s_free_inodes_count = INODES_COUNT;
    superblock.s_first_data_block  = 1;
    superblock.s_first_ino         = 1;
    superblock.s_blocks_per_group  = BLOCKS_PER_GROUP;
    superblock.s_frags_per_group   = BLOCKS_PER_GROUP;
    superblock.s_inodes_per_group  = INODES_PER_GROUP;
    superblock.s_magic             = EXT2_SUPER_MAGIC;

    memset(&bgd_table, 0, sizeof(bgd_table));
    for (uint32_t bgd = 0; bgd < GROUPS_COUNT; bgd++) {
        uint32_t start = group_meta_start(bgd);

        // block yang sudah terpakai di group ini, dihitung dari awal group
        // group 0: block 0..20 (21 block), group lain: base..base+17 (18 block)
        uint32_t used = (start - bgd * BLOCKS_PER_GROUP) + META_BLOCK_COUNT;

        bgd_table.table[bgd].bg_block_bitmap      = start;
        bgd_table.table[bgd].bg_inode_bitmap      = start + 1;
        bgd_table.table[bgd].bg_inode_table       = start + 2;
        bgd_table.table[bgd].bg_free_blocks_count = BLOCKS_PER_GROUP - used;
        bgd_table.table[bgd].bg_free_inodes_count = INODES_PER_GROUP;
        bgd_table.table[bgd].bg_used_dirs_count   = 0; // tidak di-maintain

        superblock.s_free_blocks_count += BLOCKS_PER_GROUP - used;

        // block bitmap: tandai block metadata sebagai terpakai (bit LSB dulu)
        memset(&b, 0, BLOCK_SIZE);
        for (uint32_t i = 0; i < used; i++)
            b.buf[i / 8] |= (1 << (i % 8));
        write_blocks(&b, bgd_table.table[bgd].bg_block_bitmap, 1);

        // inode bitmap dan inode table dikosongkan, siapa tahu disk bekas
        memset(&b, 0, BLOCK_SIZE);
        write_blocks(&b, bgd_table.table[bgd].bg_inode_bitmap, 1);
        for (uint32_t i = 0; i < INODES_TABLE_BLOCK_COUNT; i++)
            write_blocks(&b, bgd_table.table[bgd].bg_inode_table + i, 1);
    }

    commit_metadata();

    // root directory: inode 1, parent-nya dirinya sendiri
    uint32_t root = allocate_node();
    struct EXT2Inode root_node;
    memset(&root_node, 0, INODE_SIZE);
    root_node.i_mode = EXT2_S_IFDIR;
    init_directory_table(&root_node, root, root);
    sync_node(&root_node, root);

    commit_metadata();
}

void initialize_filesystem_ext2(void) {
    if (is_empty_storage()) {
        create_ext2();
        return;
    }

    // superblock & BGD table lebih kecil dari 1 block, jadi lewat buffer dulu
    struct BlockBuffer b;
    read_blocks(&b, 1, 1);
    memcpy(&superblock, b.buf, sizeof(superblock));
    read_blocks(&b, 2, 1);
    memcpy(&bgd_table, b.buf, sizeof(bgd_table));
}


/* ============================================================================
 * SECTION 2 - RAFI: layer block
 * ========================================================================== */

#define DIRECT_BLOCKS   12u                              // i_block[0..11]
#define PTRS_PER_BLOCK  (BLOCK_SIZE / sizeof(uint32_t))  // 128 pointer per block
#define SINGLE_LIMIT    (DIRECT_BLOCKS + PTRS_PER_BLOCK) // block ke-0..139 muat tanpa doubly

/* Jumlah block data untuk size byte, dibulatkan ke atas */
static uint32_t data_block_count(uint32_t size) {
    return (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

/* Bebaskan satu block, bitmap group-nya di-cache di buffer supaya tidak
 * baca-tulis bitmap untuk setiap block. last_bgd == GROUPS_COUNT artinya
 * buffer belum berisi bitmap apa pun. */
static void free_block_cached(uint32_t block, struct BlockBuffer *bitmap, uint32_t *last_bgd) {
    if (block == 0)
        return;

    uint32_t bgd = block / BLOCKS_PER_GROUP;
    uint32_t bit = block % BLOCKS_PER_GROUP;

    if (*last_bgd != bgd) {
        // ganti group: simpan bitmap lama dulu, baru load bitmap group baru
        if (*last_bgd < GROUPS_COUNT)
            write_blocks(bitmap, bgd_table.table[*last_bgd].bg_block_bitmap, 1);
        read_blocks(bitmap, bgd_table.table[bgd].bg_block_bitmap, 1);
        *last_bgd = bgd;
    }

    bitmap->buf[bit / 8] &= ~(1 << (bit % 8));
    bgd_table.table[bgd].bg_free_blocks_count++;
    superblock.s_free_blocks_count++;
}

uint32_t blocks_needed(uint32_t size) {
    uint32_t data = data_block_count(size);

    if (data <= DIRECT_BLOCKS)
        return data;
    if (data <= SINGLE_LIMIT)
        return data + 1; // + block single indirect

    // + block single indirect + block doubly indirect + block indirect di bawah doubly
    uint32_t rest = data - SINGLE_LIMIT;
    return data + 2 + (rest + PTRS_PER_BLOCK - 1) / PTRS_PER_BLOCK;
}

uint32_t allocate_block(uint32_t prefered_bgd) {
    struct BlockBuffer bitmap;

    // first fit, mulai dari group yang diminta, lalu group berikutnya (memutar)
    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        uint32_t bgd = (prefered_bgd + i) % GROUPS_COUNT;
        if (bgd_table.table[bgd].bg_free_blocks_count == 0)
            continue;

        uint32_t bitmap_block = bgd_table.table[bgd].bg_block_bitmap;
        read_blocks(&bitmap, bitmap_block, 1);

        for (uint32_t bit = 0; bit < BLOCKS_PER_GROUP; bit++) {
            if (bitmap.buf[bit / 8] & (1 << (bit % 8)))
                continue;

            bitmap.buf[bit / 8] |= (1 << (bit % 8));
            write_blocks(&bitmap, bitmap_block, 1);

            // free count cuma di RAM, disimpan saat commit_metadata()
            bgd_table.table[bgd].bg_free_blocks_count--;
            superblock.s_free_blocks_count--;

            return bgd * BLOCKS_PER_GROUP + bit;
        }
    }
    return 0; // disk penuh
}

/* Pemanggil wajib cek dulu: blocks_needed(node->i_size) <= superblock.s_free_blocks_count.
 * Fungsi ini tidak memanggil sync_node. */
void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd) {
    uint8_t *data  = (uint8_t *) ptr;
    uint32_t count = data_block_count(node->i_size);

    uint32_t single[PTRS_PER_BLOCK]; // isi block single indirect
    uint32_t doubly[PTRS_PER_BLOCK]; // isi block doubly indirect
    uint32_t sub[PTRS_PER_BLOCK];    // isi block indirect di bawah doubly yang sedang diisi
    uint32_t sub_block = 0;
    struct BlockBuffer last;

    memset(single, 0, BLOCK_SIZE);
    memset(doubly, 0, BLOCK_SIZE);
    memset(sub, 0, BLOCK_SIZE);
    for (uint32_t i = 0; i < 15; i++)
        node->i_block[i] = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t block = allocate_block(prefered_bgd);

        // tulis data. block terakhir yang tidak penuh disalin ke buffer
        // berisi nol, supaya tidak membaca lewat ujung buffer pemanggil
        if (i == count - 1 && node->i_size % BLOCK_SIZE != 0) {
            memset(&last, 0, BLOCK_SIZE);
            memcpy(last.buf, data + i * BLOCK_SIZE, node->i_size % BLOCK_SIZE);
            write_blocks(&last, block, 1);
        } else {
            write_blocks(data + i * BLOCK_SIZE, block, 1);
        }

        // catat pointer ke block tersebut
        if (i < DIRECT_BLOCKS) {
            node->i_block[i] = block;
        } else if (i < SINGLE_LIMIT) {
            if (i == DIRECT_BLOCKS)
                node->i_block[12] = allocate_block(prefered_bgd);
            single[i - DIRECT_BLOCKS] = block;
        } else {
            uint32_t j = i - SINGLE_LIMIT;
            if (j == 0)
                node->i_block[13] = allocate_block(prefered_bgd);

            // tiap 128 block butuh satu block indirect baru di bawah doubly
            if (j % PTRS_PER_BLOCK == 0) {
                if (sub_block != 0)
                    write_blocks(sub, sub_block, 1);
                sub_block = allocate_block(prefered_bgd);
                memset(sub, 0, BLOCK_SIZE);
                doubly[j / PTRS_PER_BLOCK] = sub_block;
            }
            sub[j % PTRS_PER_BLOCK] = block;
        }
    }

    // block pointer baru ditulis setelah semua isinya lengkap
    if (node->i_block[12] != 0)
        write_blocks(single, node->i_block[12], 1);
    if (node->i_block[13] != 0) {
        write_blocks(sub, sub_block, 1);
        write_blocks(doubly, node->i_block[13], 1);
    }

    node->i_blocks = count;
}

void read_node_data(struct EXT2Inode *node, void *buf) {
    uint8_t *data  = (uint8_t *) buf;
    uint32_t count = data_block_count(node->i_size);

    uint32_t single[PTRS_PER_BLOCK];
    uint32_t doubly[PTRS_PER_BLOCK];
    uint32_t sub[PTRS_PER_BLOCK];
    struct BlockBuffer last;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t block;

        // cari nomor block ke-i, urutannya sama persis dengan allocate_node_blocks
        if (i < DIRECT_BLOCKS) {
            block = node->i_block[i];
        } else if (i < SINGLE_LIMIT) {
            if (i == DIRECT_BLOCKS)
                read_blocks(single, node->i_block[12], 1);
            block = single[i - DIRECT_BLOCKS];
        } else {
            uint32_t j = i - SINGLE_LIMIT;
            if (j == 0)
                read_blocks(doubly, node->i_block[13], 1);
            if (j % PTRS_PER_BLOCK == 0)
                read_blocks(sub, doubly[j / PTRS_PER_BLOCK], 1);
            block = sub[j % PTRS_PER_BLOCK];
        }

        // salin tepat i_size byte, block terakhir jangan sampai lewat
        if (i == count - 1 && node->i_size % BLOCK_SIZE != 0) {
            read_blocks(&last, block, 1);
            memcpy(data + i * BLOCK_SIZE, last.buf, node->i_size % BLOCK_SIZE);
        } else {
            read_blocks(data + i * BLOCK_SIZE, block, 1);
        }
    }
}

void deallocate_blocks(void *loc, uint32_t blocks) {
    uint32_t *ptrs = (uint32_t *) loc; // = node->i_block
    struct BlockBuffer bitmap;
    uint32_t last_bgd = GROUPS_COUNT;  // belum ada bitmap yang di-load

    uint32_t direct = blocks < DIRECT_BLOCKS ? blocks : DIRECT_BLOCKS;
    deallocate_block(ptrs, direct, &bitmap, 0, &last_bgd, false);
    blocks -= direct;

    if (blocks > 0) {
        uint32_t single = blocks < PTRS_PER_BLOCK ? blocks : PTRS_PER_BLOCK;
        deallocate_block(&ptrs[12], single, &bitmap, 1, &last_bgd, true);
        blocks -= single;
    }

    if (blocks > 0)
        deallocate_block(&ptrs[13], blocks, &bitmap, 2, &last_bgd, true);

    // bitmap group terakhir masih di buffer, simpan ke disk
    if (last_bgd < GROUPS_COUNT)
        write_blocks(&bitmap, bgd_table.table[last_bgd].bg_block_bitmap, 1);
}

/* depth 0: locations[0..blocks-1] adalah block data
 * depth 1: locations[0] adalah block single indirect yang menunjuk blocks block data
 * depth 2: locations[0] adalah block doubly indirect yang (lewat indirect) menunjuk blocks block data */
uint32_t deallocate_block(uint32_t *locations, uint32_t blocks, struct BlockBuffer *bitmap, uint32_t depth, uint32_t *last_bgd, bool bgd_loaded) {
    (void) bgd_loaded; // tidak dipakai, status bitmap sudah ditandai lewat last_bgd == GROUPS_COUNT

    if (depth == 0) {
        for (uint32_t i = 0; i < blocks; i++)
            free_block_cached(locations[i], bitmap, last_bgd);
        return *last_bgd;
    }

    uint32_t ptrs[PTRS_PER_BLOCK];
    read_blocks(ptrs, locations[0], 1);

    if (depth == 1) {
        deallocate_block(ptrs, blocks, bitmap, 0, last_bgd, true);
    } else {
        for (uint32_t k = 0; blocks > 0; k++) {
            uint32_t count = blocks < PTRS_PER_BLOCK ? blocks : PTRS_PER_BLOCK;
            deallocate_block(&ptrs[k], count, bitmap, 1, last_bgd, true);
            blocks -= count;
        }
    }

    // terakhir, bebaskan block pointer itu sendiri
    free_block_cached(locations[0], bitmap, last_bgd);
    return *last_bgd;
}

bool rewrite_node_data(uint32_t inode, struct EXT2Inode *node, void *buf, uint32_t new_size) {
    // block lama nanti dibebaskan, jadi boleh dihitung sebagai ruang kosong
    uint32_t old_total = blocks_needed(node->i_size);
    if (blocks_needed(new_size) > superblock.s_free_blocks_count + old_total)
        return false;

    deallocate_blocks(node->i_block, node->i_blocks);
    node->i_size = new_size;
    allocate_node_blocks(buf, node, inode_to_bgd(inode));
    sync_node(node, inode);
    return true;
}


/* ============================================================================
 * SECTION 3 - RAPIP: layer directory + read
 * ========================================================================== */

char *get_entry_name(void *entry) {
    (void) entry;
    return NULL;
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset) {
    (void) ptr;
    (void) offset;
    return NULL;
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry) {
    (void) entry;
    return NULL;
}

uint16_t get_entry_record_len(uint8_t name_len) {
    (void) name_len;
    return 0;
}

uint32_t get_dir_first_child_offset(void *ptr) {
    (void) ptr;
    return 0;
}

int32_t dir_find(void *dir, uint32_t dir_size, const char *name, uint8_t name_len) {
    (void) dir;
    (void) dir_size;
    (void) name;
    (void) name_len;
    return -1;
}

bool dir_add_entry(void *dir, uint32_t *dir_size, uint32_t inode, const char *name, uint8_t name_len, uint8_t file_type) {
    (void) dir;
    (void) dir_size;
    (void) inode;
    (void) name;
    (void) name_len;
    (void) file_type;
    return false;
}

void dir_remove_entry(void *dir, uint32_t dir_size, uint32_t offset) {
    (void) dir;
    (void) dir_size;
    (void) offset;
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode) {
    (void) node;
    (void) inode;
    (void) parent_inode;
}

bool is_directory_empty(uint32_t inode) {
    (void) inode;
    return false;
}

int8_t read(struct EXT2DriverRequest request) {
    (void) request;
    return -1;
}

int8_t read_directory(struct EXT2DriverRequest *prequest) {
    (void) prequest;
    return -1;
}


/* ============================================================================
 * SECTION 4 - DIANDRA: write + delete
 * ========================================================================== */

int8_t write(struct EXT2DriverRequest *request) {
    (void) request;
    return -1;
}

int8_t delete(struct EXT2DriverRequest request) {
    (void) request;
    return -1;
}