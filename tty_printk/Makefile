# 指定交叉编译
ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc-13

# 模块名称
obj-m := myttyprintk.o

# 内核路径
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean