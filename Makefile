.PHONY: img_built img_extracted app_built clean

all: out.iso

out.iso: img_built
	wit cp img out.iso --region=1 --ios=36 --id=WIILNX --common-key=0 --name='Wii-Linux Installation Media' --modify=DISC,BOOT,TMD,TICKET -o
	python pack_into_iso.py

img_built: img_extracted app_built
	cp app/app.dol img/sys/main.dol

img_extracted: iso_template.iso img/sys/apploader.img

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

app_built:
	make -C app $(MAKEFLAGS)
