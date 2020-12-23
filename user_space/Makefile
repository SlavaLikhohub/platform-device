TARGET = read_data
INSTALL_PATH := /srv/nfs/busybox/test_modules

ARCH:=arm
export PATH:=/opt/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin:$(PATH)
CROSS_COMPILE:="arm-linux-gnueabihf-"

CC = gcc
CFLAGS = -O0

.PHONY:clean all

all: $(TARGET)

$(TARGET):$(TARGET).c
	$(CROSS_COMPILE)$(CC) $(CFlAGS) $^ -o $@

clean:
	rm -f $(TARGET)

install:$(INSTALL_PATH)
	sudo cp ./$(TARGET) $<

$(INSTALL_PATH):
	sudo mkdir $@