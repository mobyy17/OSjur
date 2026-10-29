#include "header/cpu/gdt.h"
#include "header/cpu/interrupt.h"

// Inisialisasi instance GDT dan GDTR global
static struct GlobalDescriptorTable global_gdt = {
    .table = {
        // Index 0: Null Descriptor
        {
            .segment_low  = 0,
            .base_low     = 0,
            .base_mid     = 0,
            .type_bit     = 0,
            .non_system   = 0,
            .privilege    = 0,
            .valid        = 0,
            .segment_high = 0,
            .flags        = 0,
            .default_bit  = 0,
            .granularity  = 0,
            .base_high    = 0
        },
        // Index 1: Kernel Code Segment Descriptor (Selector 0x08)
        {
            .segment_low  = 0xFFFF,
            .base_low     = 0,
            .base_mid     = 0,
            .type_bit     = 0xA, // Execute/Read (1010b)
            .non_system   = 1,   // Code/Data segment
            .privilege    = 0,   // Ring 0
            .valid        = 1,   // Present bit
            .segment_high = 0xF,
            .flags        = 0,
            .default_bit  = 1,   // 32-bit segment
            .granularity  = 1,   // 4 KiB granularity (Limit 4 GB)
            .base_high    = 0
        },
        // Index 2: Kernel Data Segment Descriptor (Selector 0x10)
        {
            .segment_low  = 0xFFFF,
            .base_low     = 0,
            .base_mid     = 0,
            .type_bit     = 0x2, // Read/Write (0010b)
            .non_system   = 1,   // Code/Data segment
            .privilege    = 0,   // Ring 0
            .valid        = 1,   // Present bit
            .segment_high = 0xF,
            .flags        = 0,
            .default_bit  = 1,   // 32-bit segment
            .granularity  = 1,   // 4 KiB granularity (Limit 4 GB)
            .base_high    = 0
        },
        // Index 3: User Code Segment Descriptor (Selector 0x18), sama dengan kernel code tapi DPL 3
        {
            .segment_low  = 0xFFFF,
            .base_low     = 0,
            .base_mid     = 0,
            .type_bit     = 0xA, // Execute/Read (1010b)
            .non_system   = 1,   // Code/Data segment
            .privilege    = 3,   // Ring 3
            .valid        = 1,   // Present bit
            .segment_high = 0xF,
            .flags        = 0,
            .default_bit  = 1,   // 32-bit segment
            .granularity  = 1,   // 4 KiB granularity (Limit 4 GB)
            .base_high    = 0
        },
        // Index 4: User Data Segment Descriptor (Selector 0x20), sama dengan kernel data tapi DPL 3
        {
            .segment_low  = 0xFFFF,
            .base_low     = 0,
            .base_mid     = 0,
            .type_bit     = 0x2, // Read/Write (0010b)
            .non_system   = 1,   // Code/Data segment
            .privilege    = 3,   // Ring 3
            .valid        = 1,   // Present bit
            .segment_high = 0xF,
            .flags        = 0,
            .default_bit  = 1,   // 32-bit segment
            .granularity  = 1,   // 4 KiB granularity (Limit 4 GB)
            .base_high    = 0
        },
        // Index 5: TSS Descriptor (Selector 0x28), base diisi gdt_install_tss() saat runtime
        {
            .segment_low  = sizeof(struct TSSEntry),
            .base_low     = 0,
            .base_mid     = 0,
            .type_bit     = 0x9, // 32-bit TSS (Available)
            .non_system   = 0,   // System segment
            .privilege    = 0,   // Ring 0
            .valid        = 1,   // Present bit
            .segment_high = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .flags        = 0,
            .default_bit  = 0,   // Bit 22 wajib 0 untuk TSS descriptor (Intel 3a Figure 8-3)
            .granularity  = 0,   // Limit dalam byte
            .base_high    = 0
        }
    }
};

struct GDTR _gdt_gdtr = {
    .size    = sizeof(global_gdt) - 1,
    .address = &global_gdt
};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_gdt.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_gdt.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_gdt.table[5].base_low  = base & 0xFFFF;
}