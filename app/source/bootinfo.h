// sector where the boot information will live
#define BOOTINFO_DVD_SECTOR 192

// bootInfo version
#define BOOTINFO_CUR_VERSION 1

// bootInfo magic, including trailing null
static char *bootInfoMagic = "WII-LINUX DVD  ";

typedef struct __attribute__((packed)) {
	char  magic[16];
	u32   version;

	u32   mini_nbytes;
	u32   mini_sectnum;

	u32   linux_nbytes;
	u32   linux_sectnum;
	void *linux_loadaddr;

	// if any are 0, then no initrd present
	u32   initrd_nbytes;
	u32   initrd_sectnum;
	void *initrd_loadaddr;

	// 19 more reserved fields to pad to 128 bytes (current struct uses 52)
	u32   reserved[19];
} bootInfo_t;
