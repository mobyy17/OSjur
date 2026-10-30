#ifndef _SHELL_H
#define _SHELL_H

#include <stdint.h>
#include <stdbool.h>
#include "header/filesystem/ext2.h"

/*
 * Kontrak bersama untuk semua command shell (user mode).
 *
 * Aturan untuk penulis command:
 * - Satu command satu file di src/shell/cmd-<nama>.c, signature
 *   void cmd_<nama>(int argc, char *argv[]). argv[0] adalah nama command.
 * - Semua output lewat out_*(), jangan panggil syscall puts langsung.
 *   Output dikumpulkan di buffer, REPL yang mencetaknya ke layar atau
 *   meneruskannya ke command berikutnya kalau ada pipe ("ls | grep a").
 * - Kalau command ini berada di kanan '|', pipe_in berisi output command
 *   sebelumnya sebanyak pipe_in_len byte. Kalau tidak dipipe, pipe_in == NULL.
 * - Folder sekarang ada di cwd_inode. cwd_path hanya untuk ditampilkan di prompt,
 *   jadi yang mengubah cwd_inode (cd) juga wajib memperbarui cwd_path.
 */

#define SHELL_LINE_MAX  256              // panjang maksimum satu baris input
#define SHELL_ARGV_MAX  8                // jumlah argumen maksimum per command
#define SHELL_PATH_MAX  128              // panjang maksimum cwd_path
#define SHELL_OUT_MAX   (64 * 1024)      // ukuran buffer output dan buffer pipe
#define SHELL_FILE_MAX  (64 * 1024)      // ukuran file terbesar yang bisa dibaca shell
#define SHELL_DIR_MAX   (DIR_MAX_BLOCKS * BLOCK_SIZE) // ukuran directory table terbesar (8 KiB)
#define ROOT_INODE      1

/* Sama dengan KEY_CTRL_C di keyboard.h. keyboard.h tidak di-include karena ikut membawa
 * deklarasi syscall() milik kernel yang bentrok dengan syscall() di shell. */
#define KEY_CTRL_C_CHAR 0x03

#define COLOR_TEXT      0xF  // putih
#define COLOR_PROMPT    0xA  // hijau
#define COLOR_PATH      0xB  // cyan

/* Nomor layanan syscall int 0x30, sama dengan tabel di interrupt.c */
#define SYSCALL_READ           0
#define SYSCALL_READ_DIRECTORY 1
#define SYSCALL_WRITE          2
#define SYSCALL_DELETE         3
#define SYSCALL_GETCHAR        4
#define SYSCALL_PUTCHAR        5
#define SYSCALL_PUTS           6
#define SYSCALL_KEYBOARD_ON    7

/* State shell, didefinisikan di user-shell.c */
extern uint32_t cwd_inode;
extern char     cwd_path[SHELL_PATH_MAX];
extern char     *pipe_in;
extern uint32_t pipe_in_len;

/* ===== shell-lib.c ===== */

/**
 * Panggil system call kernel lewat int 0x30.
 *
 * @param eax Nomor layanan (SYSCALL_*)
 * @param ebx Parameter pertama
 * @param ecx Parameter kedua
 * @param edx Parameter ketiga
 */
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

/**
 * Pasang buffer output yang dipakai out_*(). Dipanggil REPL sebelum menjalankan command.
 *
 * @param buf Buffer berukuran SHELL_OUT_MAX
 */
void out_init(char *buf);

/** Kosongkan buffer output (isi buffer tidak dihapus, hanya panjangnya di-reset). */
void out_reset(void);

/** @return Jumlah byte yang ada di buffer output sekarang */
uint32_t out_len(void);

/**
 * Tambah satu karakter ke buffer output. Kalau buffer penuh, karakter dibuang.
 *
 * @param c Karakter
 */
void out_putchar(char c);

/**
 * Tambah len byte dari s ke buffer output.
 *
 * @param s   Pointer data
 * @param len Jumlah byte
 */
void out_write(const char *s, uint32_t len);

/**
 * Tambah string null-terminated ke buffer output.
 *
 * @param s String
 */
void out_puts(const char *s);

/** Cetak isi buffer output ke layar lalu kosongkan buffer. */
void out_flush(void);

/**
 * Cetak string null-terminated langsung ke layar (tanpa buffer), untuk prompt dan echo input.
 *
 * @param s     String
 * @param color Warna teks
 */
void print(const char *s, uint8_t color);

/** @return Panjang string null-terminated s */
uint32_t str_len(const char *s);

/** @return true kalau a dan b sama persis */
bool str_eq(const char *a, const char *b);

/**
 * Baca directory table milik folder dir_inode (lewat syscall read_directory ke entry ".").
 *
 * @param dir_inode Inode folder
 * @param table     Buffer berukuran SHELL_DIR_MAX
 * @return          Panjang table dalam byte, 0 kalau gagal
 */
uint32_t dir_read_table(uint32_t dir_inode, uint8_t *table);

/**
 * Ambil entry aktif berikutnya di directory table. Slot kosong (inode 0) dan
 * sisa block yang belum terisi dilewati. Entry "." dan ".." ikut dikembalikan.
 * Pemakaian:
 *     uint32_t off = 0; struct EXT2DirectoryEntry *e;
 *     while ((e = dir_next_entry(table, size, &off)) != NULL) { ... }
 *
 * @param table  Directory table dari dir_read_table()
 * @param size   Panjang table
 * @param offset Posisi baca, mulai dari 0, diperbarui tiap pemanggilan
 * @return       Entry berikutnya, atau NULL kalau sudah habis
 */
struct EXT2DirectoryEntry *dir_next_entry(uint8_t *table, uint32_t size, uint32_t *offset);

/**
 * @param entry Directory entry
 * @return      Pointer ke nama entry (TIDAK null-terminated, panjangnya entry->name_len)
 */
char *entry_name(struct EXT2DirectoryEntry *entry);

/**
 * Cari entry bernama name di folder dir_inode.
 *
 * @param dir_inode Folder tempat mencari
 * @param name      Nama yang dicari (null-terminated), boleh "." atau ".."
 * @param inode     Diisi inode entry kalau ketemu
 * @param is_dir    Diisi true kalau entry adalah folder
 * @return          true kalau ketemu
 */
bool dir_lookup(uint32_t dir_inode, const char *name, uint32_t *inode, bool *is_dir);

/**
 * Baca isi file name di folder parent_inode ke buf.
 * Syscall read tidak mengembalikan ukuran file, tapi menolak buffer yang lebih kecil
 * dari file dengan kode 2. Jadi ukuran pas dicari dengan binary search pada buffer_size.
 *
 * @param parent_inode Folder tempat file
 * @param name         Nama file (null-terminated)
 * @param buf          Buffer tujuan, minimal SHELL_FILE_MAX byte
 * @param size         Diisi ukuran file dalam byte kalau berhasil
 * @return             Kode return read(): 0 berhasil, 1 folder, 2 file > SHELL_FILE_MAX, 3 tidak ada
 */
int8_t file_read(uint32_t parent_inode, const char *name, void *buf, uint32_t *size);

/* ===== Command, satu file per command di src/shell/ ===== */
void cmd_cd(int argc, char *argv[]);    // Rafi
void cmd_ls(int argc, char *argv[]);    // Rapip
void cmd_mkdir(int argc, char *argv[]); // Rafi
void cmd_cat(int argc, char *argv[]);   // Rapip
void cmd_cp(int argc, char *argv[]);    // Rapip
void cmd_rm(int argc, char *argv[]);    // Diandra
void cmd_mv(int argc, char *argv[]);    // Diandra
void cmd_find(int argc, char *argv[]);  // Lewi
void cmd_grep(int argc, char *argv[]);  // Lewi

#endif
