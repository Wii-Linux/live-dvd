// mini must be loaded here for ARMBootNow to work
#define MINI_LOAD_ADDR ((u8 *)0x91000000)

// magic number, 'MBTM' (Memory BooT Magic)
#define MEMBOOT_MAGIC 0x4D42544D

// pointer to magic
#define MEMBOOT_MAGIC_PTR ((u32 *)0x80FFFFF0)

// where to dump the kernel size for MINI to see it
// cached MEM1, a little bit under 16MB
#define MEMBOOT_SIZE_PTR ((u32 *)0x80FFFFF4)

// 2x reserved
#define MEMBOOT_RESERVED  ((u32 *)0x80FFFFF8)
#define MEMBOOT_RESERVED2 ((u32 *)0x80FFFFFC)

// FIXME: technically the kernel addr should be static too
