# Building the ISO image

## System requirements
- Wii Homebrew development environment (devkitPPC & libogc installed, environment variables set)
- `wit` (Wiimms ISO Tools) installed
- `bzip2`, `wget`, and internet access (to download the ISO template, only needed once)
- Python 3 (for `pack_into_iso.py`)

## Steps
- Build the [Wii-Linux kernel](https://github.com/Wii-Linux/wii-linux-ngx), place it as `zImage` in the top level of the project
- Build [MINI, specifically the 'dvd' branch](https://github.com/Wii-Linux/mini/tree/dvd), place it as `armboot.bin` in the top level of the project
- Build the initramfs
  - **TBD**
- Build the ArchISO rootfs
  - **TBD**
- Run `make`, which will
  - Download and extract the ISO template from static.hackmii.com
  - Build the loader app
  - Copy over the loader app and banner
  - Pack everything (MINI, kernel, initramfs, rootfs) into the image
  - Make it a valid Wii ISO
- Success!  You now have an ISO image that you can burn to disc and boot.
