![Maskot: Will Selfort - Wistoria: Wand and Sword](mascot.jpg)

# Tugas Besar IF2130 Sistem Operasi - 2026/2027

Sistem operasi x86 Protected Mode yang dikerjakan sebagai Tugas Besar IF2130 Sistem Operasi.

### Daftar Isi

- [Nama Kelompok](#nama-kelompok)
- [Cara Run](#cara-run)
- [Fitur yang Dibuat](#fitur-yang-dibuat)
- [Maskot Kelompok](#maskot-kelompok)

### Nama Kelompok

**OSjur**

| Nama | NIM | Peran |
|---|---|---|
| Muhammad Rafi Insyan Syiham Abrar | 13525022 |  |
| Muhammad Rafiif Ansyadya | 13525037 | |
| Diandra Aria Yufana | 13525113 | |
| Jonathan Lewie | 13525136 | |

### Cara Run

**Install toolchain (Ubuntu/WSL2 20.04 atau 22.04):**
```
sudo apt update
sudo apt install -y nasm gcc qemu-system-x86 make genisoimage gdb
```

**Build kernel + ISO:**
```
make build
```
(atau tekan **F5** di VSCode)

**Jalankan di QEMU:**
```
qemu-system-i386 -s -cdrom OS2025.iso
```

Kalau file system sudah aktif (Chapter 2 ke atas), tambahkan disk image:
```
qemu-system-i386 -s -S -drive file=storage.bin,format=raw,if=ide,index=0,media=disk -cdrom OS2025.iso
```

### Fitur yang Dibuat

- [x] **Chapter 0** - GDT (Null descriptor + Kernel Code/Data descriptor), kernel boot sampai QEMU tanpa triple fault
- [ ] **Chapter 1 - Framebuffer** - driver `framebuffer_write`, `framebuffer_set_cursor`, `framebuffer_clear`
- [ ] **Chapter 1 - Interrupt & Keyboard** - IDT, ISR, IRQ remap, keyboard driver
- [ ] **Chapter 2 - File System** - disk driver, EXT2-IF2130, CRUD
- [ ] **Chapter 3 - Paging & Shell** - paging, user mode, shell
- [ ] **Chapter 4 - Process** - PCB, scheduler, context switch

### Maskot Kelompok

Sunraku

