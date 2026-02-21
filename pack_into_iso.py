import os
import struct

# Constants
SECTOR_SIZE = 2048
BOOTINFO_SECTOR = 192
BOOTINFO_OFFSET = BOOTINFO_SECTOR * SECTOR_SIZE
BOOTINFO_MAGIC = b'WII-LINUX DVD  \0'
BOOTINFO_VERSION = 1

# File configurations with memory and disc locations
files = {
    "mini": {
        "filename": "armboot.bin",
        "disc_offset": 0x00062000,
        "loadaddr": None  # Not stored in bootinfo anymore
    },
    "linux": {
        "filename": "zImage",
        "disc_offset": 0x0006A000,
        "loadaddr": 0x81000000
    },
    "initrd": {
        "filename": "initrd.cpio.lz4",
        "disc_offset": 0x0086A000,
        "loadaddr": 0x00000000  # Placeholder
    },
    "rootfs": {
        "filename": "rootfs.img",
        "disc_offset": 0x0FFF0000,
        "loadaddr": None  # Not included in bootinfo
    }
}

# Calculate sector numbers and prepare embedding metadata
embed_data = {}
for key, info in files.items():
    fname = info["filename"]
    if not os.path.exists(fname):
        print("Warning: cannot open file " + fname + " - this will almost certainly not be a bootable image")
        continue

    filesize = os.path.getsize(fname)
    n_sectors = (filesize + SECTOR_SIZE - 1) // SECTOR_SIZE
    sectnum = info["disc_offset"] // SECTOR_SIZE
    embed_data[key] = {
        "filename": fname,
        "nbytes": filesize,
        "sectnum": sectnum,
        "loadaddr": info["loadaddr"]
    }

# Build bootInfo_t struct (without MINI loadaddr)
reserved = [0] * 19  # One extra since MINI loadaddr is removed
bootinfo_struct = struct.pack(
    '>16sI'       # magic + version
    'II'          # mini: nbytes, sectnum (no loadaddr)
    'III'         # linux: nbytes, sectnum, loadaddr
    'III'         # initrd: nbytes, sectnum, loadaddr
    + 'I'*19,     # reserved
    BOOTINFO_MAGIC, BOOTINFO_VERSION,
    embed_data.get("mini", {}).get("nbytes", 0),
    embed_data.get("mini", {}).get("sectnum", 0),
    embed_data.get("linux", {}).get("nbytes", 0),
    embed_data.get("linux", {}).get("sectnum", 0),
    embed_data.get("linux", {}).get("loadaddr", 0),
    embed_data.get("initrd", {}).get("nbytes", 0),
    embed_data.get("initrd", {}).get("sectnum", 0),
    embed_data.get("initrd", {}).get("loadaddr", 0),
    *reserved
)

# Create final output file with proper structure:
# - ISO9660 image (base_iso9660.iso)
# - Copy 0x0000080 bytes from Wii Disc @ offset 0x00000  
# - Copy remainder of Wii Disc image @ offset 0x40000 onwards
iso_path = "out.iso"

# Read base ISO9660 image (if exists)
base_iso_data = b""
if os.path.exists("base_iso9660.iso"):
    with open("base_iso9660.iso", "rb") as f:
        base_iso_data = f.read()

# Create output file
with open(iso_path, "wb") as out_file:
    # Write ISO9660 image data first
    if base_iso_data:
        out_file.write(base_iso_data)
    
    # Read Wii Disc input (wit_out.iso) and copy required sections
    with open("wit_out.iso", "rb") as wii_file:
        # Copy 0x0000080 bytes from Wii Disc @ offset 0x00000
        wii_file.seek(0x00000)
        wii_bytes = wii_file.read(0x0000080)
        out_file.seek(0x00000)
        out_file.write(wii_bytes)
        
        # Copy remainder of Wii Disc image from offset 0x40000 onwards  
        wii_file.seek(0x40000)
        out_file.seek(0x40000)
        remaining_data = wii_file.read()
        out_file.write(remaining_data)
    
    # Write bootinfo_struct and embed files into ISO
    if os.path.exists(iso_path):
        with open(iso_path, "r+b") as f:
            # Write bootinfo
            f.seek(BOOTINFO_OFFSET)
            f.write(bootinfo_struct)

            # Embed files
            for key, data in embed_data.items():
                with open(data["filename"], "rb") as binfile:
                    payload = binfile.read()
                    offset = data["sectnum"] * SECTOR_SIZE
                    f.seek(offset)
                    f.write(payload)
                    # Pad to sector boundary if needed
                    if len(payload) % SECTOR_SIZE != 0:
                        f.write(b'\x00' * (SECTOR_SIZE - (len(payload) % SECTOR_SIZE)))

