obj-m += kpmap.o

KERNEL_SRC := ~/Documents/65535/workspace/linux/linux_source_code/
PWD:= $(shell pwd)

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules
clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
