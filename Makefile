.PHONY: clean

all: out.iso

wit_out.iso: img/sys/main.dol
	wit cp img wit_out.iso --trunc --region=1 --ios=36 --id=WIILNX --common-key=0 --name='Wii-Linux Installation Media' --modify=DISC,BOOT,TMD,TICKET -o

out.iso: wit_out.iso base_iso9660.iso
	python3 pack_into_iso.py

img/sys/main.dol: img/sys/apploader.img app/app.dol
	cp app/app.dol img/sys/main.dol

# 'wit' breaks if the destination already exists
# if we need to regenerate the entire thing anyways,
# make sure that it's fully cleaned up first...
img/sys/apploader.img:
	rm -rf img
	wit copy iso_template.iso img --fst

iso_template.iso: iso_template.iso.bz2
	bzip2 -kdf iso_template.iso.bz2

iso_template.iso.bz2:
	wget https://static.hackmii.com/iso_template.iso.bz2

app/app.dol:
	make -C app $(MAKEFLAGS)
