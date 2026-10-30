![Maskot: Will Selfort - Wistoria: Wand and Sword](mascot.jpg)

# Tugas Besar IF2130 Sistem Operasi - 2026/2027

Sistem operasi x86 Protected Mode yang dikerjakan sebagai Tugas Besar IF2130 Sistem Operasi.

### Daftar Isi

- [Nama Kelompok](#nama-kelompok)
- [Cara Run](#cara-run)
- [Fitur yang Dibuat](#fitur-yang-dibuat)
- [Command Shell](#command-shell)
- [Desain File System](#desain-file-system)
- [Desain Paging & User Mode](#desain-paging--user-mode)
- [Maskot Kelompok](#maskot-kelompok)

### Nama Kelompok

**OSjur**

| Nama | NIM | Peran |
|---|---|---|
| Muhammad Rafi Insyan Syiham Abrar | 13525022 | Ch.1 framebuffer, Ch.2 layer block, Ch.3 inserter + simple user program + execute program, `cd`, `mkdir` |
| Muhammad Rafiif Ansyadya | 13525037 | Ch.1 keyboard driver, Ch.2 layer directory + read, Ch.3 shell REPL + pipeline, `ls`, `cat`, `cp` |
| Diandra Aria Yufana | 13525113 | Ch.1 interrupt, Ch.2 write + delete, Ch.3 GDT user + TSS, user mode, syscall `int 0x30`, `rm`, `mv` |
| Jonathan Lewie | 13525136 | Ch.2 disk driver, inisialisasi FS, layer inode, Ch.3 paging + memory manager, `find`, `grep` |

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

**Masukkan shell ke disk image.** Wajib diulang setiap kali kode shell (`src/user-shell.c`, `src/shell/`) diubah, kalau tidak shell lama yang terload:
```
make insert-shell
```

**Build kernel + ISO lalu jalankan:**
```
make build
```
lalu tekan **F5** di VSCode, atau jalankan manual dari folder `bin/`:
```
qemu-system-i386 -s -drive file=storage.bin,format=raw,if=ide,index=0,media=disk -cdrom OS2025.iso
```

Kalau muncul `Gagal membaca shell, jalankan make insert-shell`, berarti file `shell` belum ada di root directory disk image.

### Fitur yang Dibuat

- [x] **Chapter 0** - GDT (Null descriptor + Kernel Code/Data descriptor), kernel boot di QEMU tanpa triple fault
- [x] **Chapter 1 - Framebuffer** - `framebuffer_write`, `framebuffer_set_cursor`, `framebuffer_clear`
- [x] **Chapter 1 - Interrupt & Keyboard** - IRQ remap, IDT, ISR, keyboard driver
- [x] **Chapter 2 - File System** - disk driver ATA PIO, EXT2 - IF2130 Edition, `read`, `read_directory`, `write`, `delete`
- [x] **Chapter 3 - Paging** - higher half kernel, frame allocator/deallocator, free memory check
- [x] **Chapter 3 - User Mode** - GDT user descriptor + TSS, inserter, user program flat binary, masuk user mode lewat `iret`
- [x] **Chapter 3 - Shell** - syscall `int 0x30`, shell REPL dengan CWD, output ter-buffer, pipeline, 9 command wajib
- [ ] **Chapter 4 - Process** - multi page directory, PCB, process creation
- [ ] **Chapter 4 - Scheduler** - timer IRQ0, round robin, context switch
- [ ] **Chapter 4 - Multitasking** - syscall exit, `exec`, `ps`, `kill`, clock (CMOS), aplikasi Hello World

### Command Shell

Shell berjalan di user mode dan dimulai dari root. Prompt menampilkan CWD.

| Command | Keterangan |
|---|---|
| `cd <folder>` | pindah folder, termasuk `cd ..` |
| `ls` | isi folder saat ini |
| `mkdir <nama>` | buat folder |
| `cat <file>` | tampilkan isi file |
| `cp <asal> <tujuan>` | salin file |
| `rm <file>` | hapus file |
| `mv <asal> <tujuan>` | pindah atau rename file dan folder |
| `find <nama>` | cari nama di seluruh file system, mulai dari root |
| `grep <pola> [file]` | cari baris berisi pola, dari file atau dari pipeline |

Pipeline memakai `|`, contoh `cat catatan | grep os`.

Tabel syscall yang sudah dipakai:

| `eax` | Syscall |
|---|---|
| 0 | `read` |
| 1 | `read_directory` |
| 2 | `write` |
| 3 | `delete` |
| 4 | `getchar` |
| 5 | `putchar` |
| 6 | `puts` |
| 7 | aktifkan input keyboard |
| 8 | `move` |

### Desain File System

- 1 block = 512 byte, disk 4 MiB = 8192 block, dibagi 8 group @ 1024 block. Macro `GROUPS_COUNT` di kit kami beri kurung, jika tanpa kurung, `BLOCKS_PER_GROUP` jadi 256 dan 3 MiB disk tidak terpakai.
- Block 0 berisi `fs_signature`, block 1 superblock, block 2 BGD table. Metadata tiap groupnya adalah block bitmap, inode bitmap, 16 block inode table.
- Inode mulai dari 1, root = inode 1, `..` milik root menunjuk ke dirinya sendiri.
- `i_blocks` hanya menghitung block data (block pointer indirect tidak dihitung). File didukung sampai doubly indirect.
- Alokasi block memakai first fit, dimulai dari group milik parent.
- Satu folder maksimal 16 block (8 KiB) directory table.

### Desain Paging & User Mode

- Kit kami memakai page **4 KiB** (page directory + page table). Page table diakses lewat recursive mapping di entry 1023 page directory (`0xFFC00000`).
- Kernel di higher half `0xC0000000`. 4 MiB physical pertama (1024 frame) dicadangkan untuk kernel.
- Frame allocator first fit di atas frame kernel. Frame yang baru dialokasikan langsung dikosongkan supaya `.bss` program user bersih.
- Shell di load ke virtual address `0x0` dengan ukuran kurang dari 1 MiB, user stack mulai dari `0x400000 - 4`, jadi 4 MiB virtual pertama dipetakan untuk user.

### Maskot Kelompok

Sunraku

