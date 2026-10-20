![Maskot: Will Selfort - Wistoria: Wand and Sword](mascot.jpg)

# Tugas Besar IF2130 Sistem Operasi - 2026/2027

Sistem operasi x86 Protected Mode yang dikerjakan sebagai Tugas Besar IF2130 Sistem Operasi.

### Daftar Isi

- [Nama Kelompok](#nama-kelompok)
- [Cara Run](#cara-run)
- [Fitur yang Dibuat](#fitur-yang-dibuat)
- [Desain File System](#desain-file-system)
- [Maskot Kelompok](#maskot-kelompok)

### Nama Kelompok

**OSjur**

| Nama | NIM | Peran |
|---|---|---|
| Muhammad Rafi Insyan Syiham Abrar | 13525022 | Ch.1 framebuffer, Ch.2 layer block |
| Muhammad Rafiif Ansyadya | 13525037 | Ch.1 keyboard driver, Ch.2 layer directory + read |
| Diandra Aria Yufana | 13525113 | Ch.1 interrupt, Ch.2 write + delete |
| Jonathan Lewie | 13525136 | Ch.2 disk driver, inisialisasi FS, layer inode |

### Cara Run

**Install toolchain (Ubuntu/WSL2 20.04 atau 22.04):**
```
sudo apt update
sudo apt install -y nasm gcc qemu-system-x86 make genisoimage gdb
```

**Buat disk image (cukup sekali, perintah ini menghapus isi disk lama):**
```
make disk
```

**Build kernel + ISO lalu jalankan:**
```
make build
```
lalu tekan **F5** di VSCode, atau jalankan manual dari folder `bin/`:
```
qemu-system-i386 -s -drive file=storage.bin,format=raw,if=ide,index=0,media=disk -cdrom OS2025.iso
```

### Fitur yang Dibuat

- [x] **Chapter 0** - GDT (Null descriptor + Kernel Code/Data descriptor), kernel boot di QEMU tanpa triple fault
- [x] **Chapter 1 - Framebuffer** - `framebuffer_write`, `framebuffer_set_cursor`, `framebuffer_clear`
- [x] **Chapter 1 - Interrupt & Keyboard** - IRQ remap, IDT, ISR, keyboard driver
- [x] **Chapter 2 - File System** - disk driver ATA PIO, EXT2 - IF2130 Edition, `read`, `read_directory`, `write`, `delete`
- [ ] **Chapter 3 - Paging & Shell** - paging, user mode, shell
- [ ] **Chapter 4 - Process** - PCB, scheduler, context switch

### Desain File System

- 1 block = 512 byte, disk 4 MiB = 8192 block, dibagi 8 group @ 1024 block. Macro `GROUPS_COUNT` di kit kami beri kurung, jika tanpa kurung, `BLOCKS_PER_GROUP` jadi 256 dan 3 MiB disk tidak terpakai.
- Block 0 berisi `fs_signature`, block 1 superblock, block 2 BGD table. Metadata tiap groupnya adalah block bitmap, inode bitmap, 16 block inode table.
- Inode mulai dari 1, root = inode 1, `..` milik root menunjuk ke dirinya sendiri.
- `i_blocks` hanya menghitung block data (block pointer indirect tidak dihitung). File didukung sampai doubly indirect.
- Alokasi block memakai first fit, dimulai dari group milik parent.
- Satu folder maksimal 16 block (8 KiB) directory table.

### Maskot Kelompok

Sunraku

