/*
 * Main DVD loader app code.
 *
 * Copyright (C) 2025 Techflash
 *
 * Portions based on armbootnow.c
 * (https://github.com/InvoxiPlayGames/ArmbootNow/blob/master/source/armbootnow.c)
 * Copyright (C) 2024 Emma / InvoxiPlayGames (https://ipg.gay)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include "ios_hax.h"
#include "gc_dvd.h"

// sector where the boot information will live
#define BOOTINFO_DVD_SECTOR 192

// version
#define BOOTINFO_CUR_VERSION 1

// magic, including trailing null
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
}  bootInfo_t;

// mini must be loaded here
#define MINI_LOAD_ADDR ((u8 *)0x91000000)

// where to dump the kernel size for MINI to see it
// cached MEM1, a little bit under 16MB
#define LINUX_SIZE_PTR ((u32 *)0x80FFFFF0)

// DVD sector size
#define SECT_SIZE 2048

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static bootInfo_t bootInfo;
static u8 dvdbuffer[2048] ATTRIBUTE_ALIGN (32);    // One Sector

int main(int argc, char **argv) {
	int ret;
	u32 mini_nsects, linux_nsects, initrd_nsects;
	(void)argc;
	(void)argv;

	// blah blah basic libogc init you know the drill
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	puts("\x1b[2;0HWii-Linux DVD Loader v0.1, built on " __DATE__ " " __TIME__);

	// step 1, *make sure* that we have AHBPROT set before we call DVD crap
	if (!IOSHAX_ClaimPPCKERN()) {
		puts("Failed to claim PPCKERN in AHBPROT -- cannot possibly continue");
		sleep(5);
		return 1;
	}

	// Init DVD subsys
	puts("Mounting DVD, please wait...");
	init_dvd();

	// read bootinfo
	puts("Reading bootInfo...");
	ret = DVD_LowRead64(dvdbuffer, SECT_SIZE, 192 * SECT_SIZE);
	if (ret) {
		printf("DVD read bootInfo failed: %d\n", ret);
		sleep(5);
		return 1;
	}
	DCFlushRange(&dvdbuffer, sizeof(bootInfo_t)); // nuke the cache
	memcpy(&bootInfo, dvdbuffer, sizeof(bootInfo_t));

	if (memcmp(bootInfo.magic, bootInfoMagic, sizeof(bootInfo.magic))) {
		puts("Did not read a valid bootInfo block!  Bad disc?");
		sleep(5);
		return 0;
	}
	puts("bootInfo verified!");

	printf("\
DEBUG: bootInfo dump:\n\
magic: \"%s\"\n\
version: %u\n\
mini_nbytes: %u\n\
mini_sectnum: %u\n\
\n\
linux_nbytes: %u\n\
linux_sectnum: %u\n\
linux_loadaddr: %p\n\
\n\
initrd_nbytes: %u\n\
initrd_sectnum: %u\n\
initrd_loadaddr: %p\n",
	bootInfo.magic, bootInfo.version,
	bootInfo.mini_nbytes, bootInfo.mini_sectnum,
	bootInfo.linux_nbytes, bootInfo.linux_sectnum, bootInfo.linux_loadaddr,
	bootInfo.initrd_nbytes, bootInfo.initrd_sectnum, bootInfo.initrd_loadaddr);

	if (bootInfo.version != BOOTINFO_CUR_VERSION) {
		printf("Invalid bootInfo version (%u), expecting %u\n", bootInfo.version, BOOTINFO_CUR_VERSION);
		sleep(5);
		return 1;
	}

	mini_nsects = (bootInfo.mini_nbytes + (bootInfo.mini_nbytes % SECT_SIZE)) / SECT_SIZE; // round to next sector

	printf("Loading MINI (%u bytes, %u sectors) from sector %u at %p\n", bootInfo.mini_nbytes, mini_nsects, bootInfo.mini_sectnum, MINI_LOAD_ADDR);
	ret = DVD_LowRead64(MINI_LOAD_ADDR, mini_nsects * SECT_SIZE, bootInfo.mini_sectnum * SECT_SIZE);
	if (ret) {
		printf("DVD read MINI failed: %d\n", ret);
		sleep(5);
		return 1;
	}
	DCFlushRange(MINI_LOAD_ADDR, bootInfo.mini_nbytes); // nuke the cache

	puts("MINI loaded");


	linux_nsects = (bootInfo.linux_nbytes + (bootInfo.linux_nbytes % SECT_SIZE)) / SECT_SIZE; // round to next sector

	printf("Loading Linux kernel (%u bytes, %u sectors) from sector %u at %p\n", bootInfo.linux_nbytes, linux_nsects, bootInfo.linux_sectnum, bootInfo.linux_loadaddr);
	ret = DVD_LowRead64((u8 *)bootInfo.linux_loadaddr, linux_nsects * SECT_SIZE, bootInfo.linux_sectnum * SECT_SIZE);
	if (ret) {
		printf("DVD read Linux failed: %d\n", ret);
		sleep(5);
		return 1;
	}
	DCFlushRange(bootInfo.linux_loadaddr, bootInfo.linux_nbytes); // nuke the cache

	puts("Linux loaded");
	*LINUX_SIZE_PTR = bootInfo.linux_nbytes;
	DCFlushRange(LINUX_SIZE_PTR, 4);


	if (bootInfo.initrd_nbytes == 0 || bootInfo.initrd_sectnum == 0 || bootInfo.initrd_loadaddr == 0) {
		puts("skip initd");
		goto armbootnow;
	}

	initrd_nsects = (bootInfo.initrd_nbytes + (bootInfo.initrd_nbytes % SECT_SIZE)) / SECT_SIZE; // round to next sector

	printf("Loading initrd (%u bytes, %u sectors) from sector %u at %p\n", bootInfo.initrd_nbytes, initrd_nsects, bootInfo.initrd_sectnum, bootInfo.initrd_loadaddr);
	ret = DVD_LowRead64((u8 *)bootInfo.initrd_loadaddr, initrd_nsects * SECT_SIZE, bootInfo.initrd_sectnum * SECT_SIZE);
	if (ret) {
		printf("DVD read initrd failed: %d\n", ret);
		sleep(5);
		return 1;
	}

	puts("initrd loaded");

armbootnow:
	printf("DEBUG: first 5 bytes of MINI: %02X %02X %02X %02X %02X\n", MINI_LOAD_ADDR[0], MINI_LOAD_ADDR[1], MINI_LOAD_ADDR[2], MINI_LOAD_ADDR[3], MINI_LOAD_ADDR[4]);

	printf("DEBUG: first 5 bytes of Linux: %02X %02X %02X %02X %02X\n", ((u8 *)bootInfo.linux_loadaddr)[0], ((u8 *)bootInfo.linux_loadaddr)[1], ((u8 *)bootInfo.linux_loadaddr)[2], ((u8 *)bootInfo.linux_loadaddr)[3], ((u8 *)bootInfo.linux_loadaddr)[4]);

	puts("DEBUG: ARMBootNow starting, cya!");
	// ARMBootNow code starts here, credit to InvoxiPlayGames, source: https://github.com/InvoxiPlayGames/ArmbootNow/blob/master/source/armbootnow.c

	// reload IOS to give us a clean slate
	// if this fails, it doesn't matter
	printf("reloading IOS...\n");
	IOS_ReloadIOS(58);

	// run IOS exploit to get permissions
	if (IOSHAX_ClaimPPCKERN()) {
		uint32_t *sram_ffff = (uint32_t *)0xCD410000; // start of IOS SRAM

		// find the "mov pc, r0" trampoline used to launch a new IOS image
		uint32_t trampoline_addr = 0;
		for (int i = 0; i < 0x1000; i++) {
			if (sram_ffff[i] == 0xE1A0F000) {
				trampoline_addr = 0xFFFF0000 + (i * 4);
				printf("found LaunchIOS trampoline at %08x\n", trampoline_addr);
				break;
			}
		}

		// if we found it, find the pointer to aforementioned trampoline
		// this is called in the function that launches the next kernel
		uint32_t trampoline_pointer = 0;
		int trampoline_off = 0;
		if (trampoline_addr) {
			for (int i = 0; i < 0x1000; i++) {
				if (sram_ffff[i] == trampoline_addr) {
					trampoline_pointer = 0xFFFF0000 + (i * 4);
					trampoline_off = i;
					printf("found LaunchIOS trampoline pointer at %08x/%p\n", trampoline_pointer, &sram_ffff[i]);
					break;
				}
			}
		}
		// write the pointer to our code there instead
		sram_ffff[trampoline_off] = (uint32_t)MEM_K0_TO_PHYSICAL(MINI_LOAD_ADDR) + MINI_LOAD_ADDR[0];
		printf("set trampoline ptr to %08x\n", sram_ffff[trampoline_off]);

		// take the plunge...
		printf("launching...");
		sleep(2);
		printf("bye!\n");
		IOS_ReloadIOS(IOS_GetVersion());
	}

	printf("something didn't work right! exiting in 10 seconds\n");
	
	sleep(10);
	return 1;

	// ARMBootNow Code Ends
}
