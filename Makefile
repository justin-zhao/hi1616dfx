KSRC = /usr/src/linux-headers-4.10.0-42-generic/
#KSRC = /lib/modules/4.10.0-42-generic/build
#KSRC = /home/z00228467/file/kernel-dev
PWD = $(shell pwd)

#EXTRA_CFLAGS := -DMMAN -I${pwd}/include
#export EXTRA_CFLAGS

MODULE := dfx
obj-m := dfx.o
dfx-objs := djtag/djtag.o proc/LLC.o proc/HHA.o proc/AA.o proc/PA.o proc/proc.o 
 
default:
	./processType.sh $(KSRC)/include/linux/fs.h
	$(MAKE) -C $(KSRC) M=$(PWD) LDDINC=$(PWD)/../include
	#$(MAKE) -C $(KSRC) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=$(PWD) LDDINC=$(PWD)/../include
clean:
	$(MAKE) -C $(KSRC) ARCH=arm64 M=$(PWD) clean
	#$(MAKE) -C $(KSRC) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=$(PWD) clean
