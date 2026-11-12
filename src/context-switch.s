global process_context_switch

; Offset field struct Context (lihat header/process/process.h dan CPURegister di interrupt.h)
CTX_EDI    equ 0
CTX_ESI    equ 4
CTX_EBP    equ 8
CTX_ESP    equ 12
CTX_EBX    equ 16
CTX_EDX    equ 20
CTX_ECX    equ 24
CTX_EAX    equ 28
CTX_GS     equ 32
CTX_FS     equ 36
CTX_ES     equ 40
CTX_DS     equ 44
CTX_EIP    equ 48
CTX_EFLAGS equ 52

USER_CODE_SELECTOR equ 0x18 | 0x3
USER_DATA_SELECTOR equ 0x20 | 0x3

section .text
; void process_context_switch(struct Context ctx)
; ctx dikirim by value, jadi isinya ada di [esp + 4] (cdecl, [esp] = return address)
process_context_switch:
    ; 1. Simpan base address ctx
    lea  ecx, [esp + 4]

    ; 2. Stack untuk iret (inter-privilege): ss, esp, eflags, cs, eip
    push USER_DATA_SELECTOR
    push dword [ecx + CTX_ESP]
    push dword [ecx + CTX_EFLAGS]
    push USER_CODE_SELECTOR
    push dword [ecx + CTX_EIP]

    ; Segment register di-push dulu, di-load paling akhir (langkah 4)
    push dword [ecx + CTX_GS]
    push dword [ecx + CTX_FS]
    push dword [ecx + CTX_ES]
    push dword [ecx + CTX_DS]

    ; 3. Load general purpose & index register, ecx paling akhir karena dipakai sebagai pointer
    mov  edi, [ecx + CTX_EDI]
    mov  esi, [ecx + CTX_ESI]
    mov  ebp, [ecx + CTX_EBP]
    mov  ebx, [ecx + CTX_EBX]
    mov  edx, [ecx + CTX_EDX]
    mov  eax, [ecx + CTX_EAX]
    mov  ecx, [ecx + CTX_ECX]

    ; 4. Load segment register
    pop  ds
    pop  es
    pop  fs
    pop  gs

    ; 5. Lompat ke process
    iret