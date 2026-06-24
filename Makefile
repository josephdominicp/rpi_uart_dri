# Define the target kernel module binary
obj-m += src/rpi_uart_driver.o

# PATH TO KERNEL HEADERS: 
# Replace this path with the local directory where you cloned/configured 
# your target Raspberry Pi kernel source tree on your host machine.
KDIR ?= /path/to/your/raspberrypi/kernel/linux

PWD := $(shell pwd)

# Explicit target constraints for the Raspberry Pi cross-compiler
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean