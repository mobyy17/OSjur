#include "header/shell/shell.h"

/* Helper bersama untuk shell dan semua command. Lihat dokumentasi di header/shell/shell.h */

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    // Register diisi lewat constraint "a", "b", "c", "d" supaya gcc tidak memakai
    // salah satunya sebagai register perantara di antara instruksi mov.
    __asm__ volatile("int $0x30" : /* <Empty> */ : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx) : "memory");
}

/* ===== Buffer output ===== */
static char     *out_buf  = 0;
static uint32_t out_count = 0;

void out_init(char *buf) {
    out_buf   = buf;
    out_count = 0;
}

void out_reset(void) {
    out_count = 0;
}

uint32_t out_len(void) {
    return out_count;
}

void out_putchar(char c) {
    if (out_count < SHELL_OUT_MAX)
        out_buf[out_count++] = c;
}

void out_write(const char *s, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        out_putchar(s[i]);
}

void out_puts(const char *s) {
    out_write(s, str_len(s));
}

void out_flush(void) {
    if (out_count > 0)
        syscall(SYSCALL_PUTS, (uint32_t) out_buf, out_count, COLOR_TEXT);
    out_count = 0;
}

void print(const char *s, uint8_t color) {
    syscall(SYSCALL_PUTS, (uint32_t) s, str_len(s), color);
}

/* ===== String ===== */
uint32_t str_len(const char *s) {
    uint32_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

bool str_eq(const char *a, const char *b) {
    uint32_t i = 0;
    while (a[i] != '\0' && a[i] == b[i])
        i++;
    return a[i] == b[i];
}

/* ===== Directory dan file ===== */
uint32_t dir_read_table(uint32_t dir_inode, uint8_t *table) {
    // Entry "." di dalam folder menunjuk ke folder itu sendiri,
    // jadi read_directory(parent = folder, name = ".") mengembalikan table folder itu.
    struct EXT2DriverRequest request = {
        .buf          = table,
        .name         = ".",
        .name_len     = 1,
        .parent_inode = dir_inode,
        .buffer_size  = SHELL_DIR_MAX,
        .is_directory = true,
    };
    int8_t retcode = -1;
    syscall(SYSCALL_READ_DIRECTORY, (uint32_t) &request, (uint32_t) &retcode, 0);
    if (retcode != 0)
        return 0;
    return request.buffer_size; // diisi kernel dengan i_size folder
}

struct EXT2DirectoryEntry *dir_next_entry(uint8_t *table, uint32_t size, uint32_t *offset) {
    uint32_t header = sizeof(struct EXT2DirectoryEntry);
    while (*offset < size) {
        uint32_t left = BLOCK_SIZE - *offset % BLOCK_SIZE; // sisa byte di block ini
        struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *) (table + *offset);

        if (left < header || entry->rec_len == 0) {
            *offset += left; // sisa block kosong, lompat ke block berikutnya
            continue;
        }
        *offset += entry->rec_len;
        if (entry->inode != 0)
            return entry;
    }
    return 0;
}

char *entry_name(struct EXT2DirectoryEntry *entry) {
    return (char *) entry + sizeof(struct EXT2DirectoryEntry);
}

bool dir_lookup(uint32_t dir_inode, const char *name, uint32_t *inode, bool *is_dir) {
    uint8_t table[SHELL_DIR_MAX];
    uint32_t size = dir_read_table(dir_inode, table);
    uint32_t name_len = str_len(name);

    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry;
    while ((entry = dir_next_entry(table, size, &offset)) != 0) {
        if (entry->name_len != name_len)
            continue;
        bool same = true;
        for (uint32_t i = 0; i < name_len; i++)
            if (entry_name(entry)[i] != name[i])
                same = false;
        if (same) {
            *inode  = entry->inode;
            *is_dir = entry->file_type == EXT2_FT_DIR;
            return true;
        }
    }
    return false;
}

static int8_t read_with_size(uint32_t parent_inode, const char *name, void *buf, uint32_t buffer_size) {
    struct EXT2DriverRequest request = {
        .buf          = buf,
        .name         = (char *) name,
        .name_len     = str_len(name),
        .parent_inode = parent_inode,
        .buffer_size  = buffer_size,
        .is_directory = false,
    };
    int8_t retcode = -1;
    syscall(SYSCALL_READ, (uint32_t) &request, (uint32_t) &retcode, 0);
    return retcode;
}

int8_t file_read(uint32_t parent_inode, const char *name, void *buf, uint32_t *size) {
    // Baca dulu dengan buffer terbesar. Kalau gagal, kode error read() langsung diteruskan.
    int8_t retcode = read_with_size(parent_inode, name, buf, SHELL_FILE_MAX);
    if (retcode != 0)
        return retcode;

    // Cari buffer_size terkecil yang masih diterima read(), itulah ukuran file
    uint32_t low = 0, high = SHELL_FILE_MAX;
    while (low < high) {
        uint32_t mid = (low + high) / 2;
        if (read_with_size(parent_inode, name, buf, mid) == 0)
            high = mid;
        else
            low = mid + 1;
    }
    *size = high;
    return 0;
}
