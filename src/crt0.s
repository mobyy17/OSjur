global _start
extern main

section .text
_start:
    call main
    ; main selesai: keluar lewat syscall exit (10), return code main di ebx
    mov  ebx, eax
    mov  eax, 10
    int  0x30
    ; pengaman, tidak seharusnya sampai sini
    jmp  $
