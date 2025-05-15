# Wii-Linux DVD Install Media
This project consists of the build tools, and the loader app itself.  The latter is a libogc-based app that loads MINI, Linux, and an initramfs from a specially crafted DVD.  The former is what creates the DVD.

## Current progress
- DVD reads work
- ARMBootNow works
- Customized MINI works
- Linux boots (partially, it gets past the zImage wrapper before panicing doing USB stuff)
- Initramfs does not yet exist
- ArchISO / userspace tooling does not yet exist

## TODO
- Fix kernel panic
- Make rvl-di work again (see also [this issue tracking it](https://github.com/Wii-Linux/wii-linux-ngx/issues/6))
- ArchISO config and initramfs hacks to support it
- Cleaner loading screen
- Migrate from libogc to [PowerBlocks](https://github.com/rainbain/PowerBlocks) if it ever gains IOS IPC

## Copyright
Loader / boot code Copyright (C) Techflash 2025  
ARMBootNow code Copyright (C) 2024 Emma / InvoxiPlayGames (https://ipg.gay)  
Exploit code Copyright (C) 2024 Emma / InvoxiPlayGames (https://ipg.gay), based on the work of https://github.com/mkwcat  
DVD I/O code Copyright (C) 2007, 2008, 2009, 2010 emu_kidid  
Banner graphics and related Wii-Linux branding graphics Copyright (C) 2024, 2025 Tech64 http://pretzels.onthewifi.com/  
Banner audio "It's A Newbuntu" Copyright (C) Sam Hulick, CC-BY - modified by sirocyl  

As necessitated by the DVD I/O code, this project is licensed under the GNU GPL v2.
