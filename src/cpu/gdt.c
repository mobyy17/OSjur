#include "../header/cpu/gdt.h"

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
        }
    }
};

struct GDTR _gdt_gdtr = {
    .size    = sizeof(global_gdt) - 1,
    .address = &global_gdt
};