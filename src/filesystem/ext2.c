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

/* Signature disamakan dengan kit supaya sample-image.bin yang terbaru dikenali dan tidak diformat ulang oleh create_ext2() (Book 2.6.4). */
const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

/* State file system di RAM, di-load dari block 1 dan 2 */
static struct EXT2Superblock superblock;
static struct EXT2BlockGroupDescriptorTable bgd_table;

/* SECTION 1 - LEWI: fondasi + layer inode */
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

/* SECTION 2 - RAFI: layer block */
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

/* SECTION 3 - RAPIP: layer directory + read */
#define DIR_MAX_SIZE (DIR_MAX_BLOCKS * BLOCK_SIZE) // 8 KiB, ukuran buffer directory di stack
#define ENTRY_HEADER sizeof(struct EXT2DirectoryEntry) // 9 byte, nama langsung setelahnya

char *get_entry_name(void *entry) {
    return (char *) entry + ENTRY_HEADER;
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset) {
    return (struct EXT2DirectoryEntry *) ((uint8_t *) ptr + offset);
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry) {
    return (struct EXT2DirectoryEntry *) ((uint8_t *) entry + entry->rec_len);
}

uint16_t get_entry_record_len(uint8_t name_len) {
    uint16_t len = ENTRY_HEADER + name_len;
    return (len + 3) / 4 * 4; // bulatkan ke kelipatan 4
}

uint32_t get_dir_first_child_offset(void *ptr) {
    // lewati entry "." dan ".."
    struct EXT2DirectoryEntry *self = get_directory_entry(ptr, 0);
    return self->rec_len + get_next_directory_entry(self)->rec_len;
}

/* Lewati posisi yang bukan entry aktif, return dir_size kalau sudah habis:
 * - sisa block kosong (rec_len 0): lompat ke awal block berikutnya
 * - entry inode 0 (slot kosong): lompat sejauh rec_len. Reference implementation (sample-image.bin) menutup sisa block dengan entry seperti ini. */
static uint32_t skip_empty(void *dir, uint32_t dir_size, uint32_t offset) {
    while (offset < dir_size) {
        uint32_t left = BLOCK_SIZE - offset % BLOCK_SIZE;
        if (left < ENTRY_HEADER) {
            offset += left;
            continue;
        }
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir, offset);
        if (entry->rec_len == 0)
            offset += left;
        else if (entry->inode == 0)
            offset += entry->rec_len;
        else
            break;
    }
    return offset;
}

/* Jumlah byte terpakai di block yang dimulai dari block_start, dihitung sampai ujung entry aktif (inode != 0) terakhir. Slot inode 0 di belakangnya dianggap ruang kosong. */
static uint32_t block_used(void *dir, uint32_t block_start) {
    uint32_t pos = 0, used = 0;
    while (pos + ENTRY_HEADER <= BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir, block_start + pos);
        if (entry->rec_len == 0)
            break;
        pos += entry->rec_len;
        if (entry->inode != 0)
            used = pos;
    }
    return used;
}

int32_t dir_find(void *dir, uint32_t dir_size, const char *name, uint8_t name_len) {
    uint32_t offset = skip_empty(dir, dir_size, 0);
    while (offset < dir_size) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir, offset);
        if (entry->name_len == name_len && memcmp(get_entry_name(entry), name, name_len) == 0)
            return offset;
        offset = skip_empty(dir, dir_size, offset + entry->rec_len);
    }
    return -1;
}

bool dir_add_entry(void *dir, uint32_t *dir_size, uint32_t inode, const char *name, uint8_t name_len, uint8_t file_type) {
    uint16_t need = get_entry_record_len(name_len);
    uint32_t offset = *dir_size; // default: block baru di ujung

    // cari block yang sisa ruangnya cukup
    for (uint32_t start = 0; start < *dir_size; start += BLOCK_SIZE) {
        uint32_t used = block_used(dir, start);
        if (BLOCK_SIZE - used >= need) {
            offset = start + used;
            // buang sisa slot kosong di belakang
            memset((uint8_t *) dir + offset, 0, BLOCK_SIZE - used);
            break;
        }
    }

    if (offset == *dir_size) {
        if (*dir_size >= DIR_MAX_SIZE)
            return false; // directory sudah 16 block
        memset((uint8_t *) dir + *dir_size, 0, BLOCK_SIZE);
        *dir_size += BLOCK_SIZE;
    }

    struct EXT2DirectoryEntry *entry = get_directory_entry(dir, offset);
    memset(entry, 0, need);
    entry->inode     = inode;
    entry->rec_len   = need;
    entry->name_len  = name_len;
    entry->file_type = file_type;
    memcpy(get_entry_name(entry), name, name_len);
    return true;
}

void dir_remove_entry(void *dir, uint32_t dir_size, uint32_t offset) {
    // "." dan ".." tidak boleh dihapus
    if (offset < get_dir_first_child_offset(dir) || offset >= dir_size)
        return;

    uint32_t block_start = offset / BLOCK_SIZE * BLOCK_SIZE;
    uint32_t end         = block_start + block_used(dir, block_start);
    uint16_t rec_len     = get_directory_entry(dir, offset)->rec_len;

    // geser entry sesudahnya ke kiri, lalu nolkan sisa block supaya tidak ada lubang
    uint8_t *p = (uint8_t *) dir;
    memmove(p + offset, p + offset + rec_len, end - (offset + rec_len));
    memset(p + end - rec_len, 0, block_start + BLOCK_SIZE - (end - rec_len));
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode) {
    uint8_t table[BLOCK_SIZE];
    uint32_t size = 0;

    dir_add_entry(table, &size, inode, ".", 1, EXT2_FT_DIR);
    dir_add_entry(table, &size, parent_inode, "..", 2, EXT2_FT_DIR);

    node->i_mode = EXT2_S_IFDIR;
    node->i_size = size; // 512, satu block
    allocate_node_blocks(table, node, inode_to_bgd(inode));
}

/* Baca isi directory table milik inode ke buf (minimal DIR_MAX_SIZE byte) */
static bool read_dir_table(uint32_t inode, void *buf, uint32_t *size) {
    struct EXT2Inode node;
    read_inode(inode, &node);
    if (node.i_size > DIR_MAX_SIZE)
        return false;
    read_node_data(&node, buf);
    *size = node.i_size;
    return true;
}

bool is_directory_empty(uint32_t inode) {
    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(inode, table, &size))
        return false;

    // kosong = tidak ada entry lagi setelah "." dan ".."
    return skip_empty(table, size, get_dir_first_child_offset(table)) >= size;
}

int8_t read(struct EXT2DriverRequest request) {
    if (!is_directory_inode(request.parent_inode))
        return 4;

    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(request.parent_inode, table, &size))
        return -1;

    int32_t offset = dir_find(table, size, request.name, request.name_len);
    if (offset < 0)
        return 3;

    struct EXT2DirectoryEntry *entry = get_directory_entry(table, offset);
    if (entry->file_type == EXT2_FT_DIR)
        return 1;

    struct EXT2Inode node;
    read_inode(entry->inode, &node);
    if (request.buffer_size < node.i_size)
        return 2;

    read_node_data(&node, request.buf);
    return 0;
}

int8_t read_directory(struct EXT2DriverRequest *prequest) {
    if (!is_directory_inode(prequest->parent_inode))
        return 3;

    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(prequest->parent_inode, table, &size))
        return -1;

    int32_t offset = dir_find(table, size, prequest->name, prequest->name_len);
    if (offset < 0)
        return 2;

    struct EXT2DirectoryEntry *entry = get_directory_entry(table, offset);
    if (entry->file_type != EXT2_FT_DIR)
        return 1;

    struct EXT2Inode node;
    read_inode(entry->inode, &node);
    if (prequest->buffer_size < node.i_size)
        return -1;

    read_node_data(&node, prequest->buf);
    prequest->buffer_size = node.i_size; // supaya pemanggil tahu panjang tabelnya
    return 0;
}


/* SECTION 4 - DIANDRA: write + delete */

int8_t write(struct EXT2DriverRequest *request) {
    if (!is_directory_inode(request->parent_inode))
        return 2;

    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(request->parent_inode, table, &size))
        return -1;

    if (dir_find(table, size, request->name, request->name_len) >= 0)
        return 1;
    if (request->name_len == 0)
        return -1;

    // cek sisa disk dulu. +1 cadangan kalau daftar isi parent butuh block baru
    uint32_t need = request->is_directory ? 1 : blocks_needed(request->buffer_size);
    if (superblock.s_free_inodes_count == 0 || need + 1 > superblock.s_free_blocks_count)
        return -1;

    uint32_t inode = allocate_node();
    struct EXT2Inode node;
    memset(&node, 0, sizeof(node));
    uint8_t file_type;

    if (request->is_directory) {
        init_directory_table(&node, inode, request->parent_inode);
        file_type = EXT2_FT_DIR;
    } else {
        node.i_mode = EXT2_S_IFREG;
        node.i_size = request->buffer_size;
        allocate_node_blocks(request->buf, &node, inode_to_bgd(request->parent_inode));
        file_type = EXT2_FT_REG_FILE;
    }
    sync_node(&node, inode);

    // daftarkan nama baru di parent, lalu simpan daftar isi parent ke disk
    struct EXT2Inode parent;
    read_inode(request->parent_inode, &parent);
    if (!dir_add_entry(table, &size, inode, request->name, request->name_len, file_type) ||
        !rewrite_node_data(request->parent_inode, &parent, table, size)) {
        deallocate_node(inode); // gagal, batalkan inode yang tadi dibuat
        commit_metadata();
        return -1;
    }

    commit_metadata();
    return 0;
}

int8_t delete(struct EXT2DriverRequest request) {
    if (!is_directory_inode(request.parent_inode))
        return 3;

    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(request.parent_inode, table, &size))
        return -1;

    int32_t offset = dir_find(table, size, request.name, request.name_len);
    if (offset < 0)
        return 1;
    if ((uint32_t) offset < get_dir_first_child_offset(table))
        return -1; // "." dan ".." tidak boleh dihapus

    struct EXT2DirectoryEntry *entry = get_directory_entry(table, offset);
    bool is_dir = entry->file_type == EXT2_FT_DIR;
    if (is_dir != request.is_directory)
        return 1; // nama ada, tapi tipenya beda dengan yang diminta

    uint32_t inode = entry->inode;
    if (is_dir && !is_directory_empty(inode))
        return 2;

    deallocate_node(inode);

    // hapus nama dari daftar isi parent, lalu simpan ke disk
    struct EXT2Inode parent;
    read_inode(request.parent_inode, &parent);
    dir_remove_entry(table, size, offset);
    rewrite_node_data(request.parent_inode, &parent, table, size);

    commit_metadata();
    return 0;
}

/* Inode parent dari folder dir_inode, dibaca dari entry ".." (entry kedua) */
static uint32_t parent_of(uint32_t dir_inode) {
    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(dir_inode, table, &size))
        return 0;
    return get_next_directory_entry(get_directory_entry(table, 0))->inode;
}

/* Ganti isi entry ".." milik folder dir_inode menjadi new_parent */
static void set_parent_entry(uint32_t dir_inode, uint32_t new_parent) {
    uint8_t table[DIR_MAX_SIZE];
    uint32_t size;
    if (!read_dir_table(dir_inode, table, &size))
        return;
    get_next_directory_entry(get_directory_entry(table, 0))->inode = new_parent;

    struct EXT2Inode node;
    read_inode(dir_inode, &node);
    rewrite_node_data(dir_inode, &node, table, size);
}

int8_t move(struct EXT2DriverRequest src, struct EXT2DriverRequest dst) {
    if (!is_directory_inode(src.parent_inode) || !is_directory_inode(dst.parent_inode))
        return 3;
    if (dst.name_len == 0)
        return -1;

    uint8_t src_table[DIR_MAX_SIZE];
    uint32_t src_size;
    if (!read_dir_table(src.parent_inode, src_table, &src_size))
        return -1;

    int32_t offset = dir_find(src_table, src_size, src.name, src.name_len);
    if (offset < 0)
        return 1;
    if ((uint32_t) offset < get_dir_first_child_offset(src_table))
        return -1; // "." dan ".." tidak boleh dipindah

    struct EXT2DirectoryEntry *entry = get_directory_entry(src_table, offset);
    uint32_t inode     = entry->inode;
    uint8_t  file_type = entry->file_type;

    // folder tidak boleh dipindah ke dalam dirinya sendiri atau turunannya (mv a a/b)
    if (file_type == EXT2_FT_DIR) {
        uint32_t cur = dst.parent_inode;
        while (cur != 0) {
            if (cur == inode)
                return -1;
            if (cur == 1)
                break; // sudah sampai root
            cur = parent_of(cur);
        }
    }

    // rename di folder yang sama: cukup satu directory table
    if (src.parent_inode == dst.parent_inode) {
        if (dir_find(src_table, src_size, dst.name, dst.name_len) >= 0)
            return 2;
        dir_remove_entry(src_table, src_size, offset);
        if (!dir_add_entry(src_table, &src_size, inode, dst.name, dst.name_len, file_type))
            return -1;
        struct EXT2Inode parent;
        read_inode(src.parent_inode, &parent);
        rewrite_node_data(src.parent_inode, &parent, src_table, src_size);
        commit_metadata();
        return 0;
    }

    // pindah folder: tambah dulu di tujuan, baru hapus dari asal
    uint8_t dst_table[DIR_MAX_SIZE];
    uint32_t dst_size;
    if (!read_dir_table(dst.parent_inode, dst_table, &dst_size))
        return -1;
    if (dir_find(dst_table, dst_size, dst.name, dst.name_len) >= 0)
        return 2;
    if (!dir_add_entry(dst_table, &dst_size, inode, dst.name, dst.name_len, file_type))
        return -1;

    struct EXT2Inode dst_parent;
    read_inode(dst.parent_inode, &dst_parent);
    if (!rewrite_node_data(dst.parent_inode, &dst_parent, dst_table, dst_size))
        return -1;

    struct EXT2Inode src_parent;
    read_inode(src.parent_inode, &src_parent);
    dir_remove_entry(src_table, src_size, offset);
    rewrite_node_data(src.parent_inode, &src_parent, src_table, src_size);

    // folder yang dipindah harus menunjuk parent barunya lewat ".."
    if (file_type == EXT2_FT_DIR)
        set_parent_entry(inode, dst.parent_inode);

    commit_metadata();
    return 0;
}
