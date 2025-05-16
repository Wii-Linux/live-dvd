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
#include <fat.h>
#include "ios_hax.h"
#include "gc_dvd.h"
#include "bootinfo.h"
#include "mini_memboot.h"

// Enable booting from SD with a stub bootInfo for development/debugging purporses
// requires:
//  - bootmii/armboot.bin (MINI)
//  - wiilinux/dvd.krn (kernel)
//  - tbd (initramfs)
#define SD_BOOT_MODE

// DVD sector size
#define SECT_SIZE 2048

#ifndef SD_BOOT_MODE
// scratch variable to read the bootInfo into
static bootInfo_t bootInfo;
#else
static bootInfo_t bootInfo = {
	.magic = { 'W', 'I', 'I', '-', 'L', 'I', 'N', 'U', 'X', ' ', 'D', 'V', 'D', ' ', ' ', '\0' },
	.version = BOOTINFO_CUR_VERSION,
	.linux_loadaddr = (void *)0x81000000,
};
#endif

// aligned buffer to read sectors from DVD into
static u8 dvdbuffer[2048] ATTRIBUTE_ALIGN (32);

// macro to handle dvd failures
#define HANDLE_DVD_FAIL(name) \
	if (ret) { \
		printf("DVD read " name " failed: %d\n", ret); \
		sleep(5); \
		return 1; \
	}

#define die(cond, ...) \
	if (cond) { \
		printf(__VA_ARGS__); \
		sleep(5); \
		return 1; \
	}

int main(int argc, char **argv) {
	int ret; // scratch var for return values
	u32 nsects; // number of sectors for a read

	// libogc GX junk
	void *xfb;
	GXRModeObj *rmode;

	#ifdef SD_BOOT_MODE
	FILE *fp;
	#endif

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
	die(!IOSHAX_ClaimPPCKERN(), "Failed to claim PPCKERN in AHBPROT -- cannot possibly continue\n");

	// Init DVD subsys
	#ifndef SD_BOOT_MODE
	puts("Mounting DVD, please wait...");
	init_dvd();

	// read bootinfo
	puts("Reading bootInfo...");
	ret = DVD_LowRead64(dvdbuffer, SECT_SIZE, 192 * SECT_SIZE);
	HANDLE_DVD_FAIL("bootInfo");
	DCFlushRange(&dvdbuffer, sizeof(bootInfo_t)); // nuke the cache
	memcpy(&bootInfo, dvdbuffer, sizeof(bootInfo_t));
	#endif

	die(memcmp(bootInfo.magic, bootInfoMagic, sizeof(bootInfo.magic)) != 0,
		"Did not read a valid bootInfo block!  Bad disc?\n");
	puts("bootInfo verified!");

	die(bootInfo.version != BOOTINFO_CUR_VERSION,
		"Invalid bootInfo version (%u), expecting %u\n", bootInfo.version, BOOTINFO_CUR_VERSION);

	nsects = (bootInfo.mini_nbytes + (bootInfo.mini_nbytes % SECT_SIZE)) / SECT_SIZE; // round to next sector

	printf("Loading MINI (%u bytes, %u sectors) from sector %u at %p\n", bootInfo.mini_nbytes, nsects, bootInfo.mini_sectnum, MINI_LOAD_ADDR);
	#ifndef SD_BOOT_MODE
	ret = DVD_LowRead64(MINI_LOAD_ADDR, nsects * SECT_SIZE, bootInfo.mini_sectnum * SECT_SIZE);
	HANDLE_DVD_FAIL("MINI");
	#else
	// do fat init, since this is the first file
	ret = fatInitDefault();
	die(!ret, "Failed to init SD card: %d\n", ret);
	chdir("/");

	fp = fopen("/bootmii/armboot.bin", "rb");
	die(!fp, "Failed to open armboot.bin\n");

	// get filesize
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// read the file
	ret = fread(MINI_LOAD_ADDR, 1, size, fp);
	fclose(fp);

	die(ret != (int)size, "Failed to read armboot.bin: %d\n", ret);
	bootInfo.mini_nbytes = size;
	#endif

	DCFlushRange(MINI_LOAD_ADDR, bootInfo.mini_nbytes); // nuke the cache
	puts("MINI loaded");

	nsects = (bootInfo.linux_nbytes + (bootInfo.linux_nbytes % SECT_SIZE)) / SECT_SIZE; // round to next sector

	printf("Loading Linux kernel (%u bytes, %u sectors) from sector %u at %p\n", bootInfo.linux_nbytes, nsects, bootInfo.linux_sectnum, bootInfo.linux_loadaddr);
	#ifndef SD_BOOT_MODE
	ret = DVD_LowRead64((u8 *)bootInfo.linux_loadaddr, nsects * SECT_SIZE, bootInfo.linux_sectnum * SECT_SIZE);
	HANDLE_DVD_FAIL("Linux");
	#else
	fp = fopen("/wiilinux/dvd.krn", "rb");
	die(!fp, "Failed to open dvd.krn\n");

	// get filesize
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// read the file
	ret = fread((u8 *)bootInfo.linux_loadaddr, 1, size, fp);
	fclose(fp);

	die(ret != (int)size, "Failed to read dvd.krn: %d\n", ret);
	bootInfo.linux_nbytes = size;
	#endif

	DCFlushRange(bootInfo.linux_loadaddr, bootInfo.linux_nbytes); // nuke the cache

	puts("Linux loaded");
	*MEMBOOT_MAGIC_PTR = MEMBOOT_MAGIC;
	*MEMBOOT_SIZE_PTR = bootInfo.linux_nbytes;
	DCFlushRange(MEMBOOT_MAGIC_PTR, 8);

	if (bootInfo.initrd_nbytes == 0 || bootInfo.initrd_sectnum == 0 || bootInfo.initrd_loadaddr == 0) {
		puts("skip initd");
		goto armbootnow;
	}

	nsects = (bootInfo.initrd_nbytes + (bootInfo.initrd_nbytes % SECT_SIZE)) / SECT_SIZE; // round to next sector

	printf("Loading initrd (%u bytes, %u sectors) from sector %u at %p\n", bootInfo.initrd_nbytes, nsects, bootInfo.initrd_sectnum, bootInfo.initrd_loadaddr);
	#ifndef SD_BOOT_MODE
	ret = DVD_LowRead64((u8 *)bootInfo.initrd_loadaddr, nsects * SECT_SIZE, bootInfo.initrd_sectnum * SECT_SIZE);
	HANDLE_DVD_FAIL("initrd");
	#else
	fp = fopen("/wiilinux/dvdramfs.img", "rb");
	die(!fp, "Failed to open dvdramfs.img\n");

	// get filesize
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// read the file
	ret = fread((u8 *)bootInfo.initrd_loadaddr, 1, size, fp);
	fclose(fp);

	die(ret != (int)size, "Failed to read dvdramfs.img: %d\n", ret);
	bootInfo.initrd_nbytes = size;
	#endif

	puts("initrd loaded");

armbootnow:
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
