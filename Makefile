NASM ?= nasm
CC   ?= x86_64-elf-gcc
LD   ?= x86_64-elf-ld
QEMU ?= qemu-system-x86_64

BUILD := build
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffreestanding -fno-stack-protector \
          -fno-pic -mno-red-zone -mcmodel=kernel
LDFLAGS := -T kernel/linker.ld -nostdlib

all: $(BUILD)/hadeed.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot/bootloader.asm | $(BUILD)
	$(NASM) -f bin $< -o $@
	@test $$(wc -c < $@) -eq 512
	@test "$$(tail -c 2 $@ | od -An -tx1 | tr -d ' \n')" = 55aa

$(BUILD)/entry.o: kernel/entry.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/main.o: kernel/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/entry.o $(BUILD)/main.o kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/entry.o $(BUILD)/main.o

$(BUILD)/hadeed.img: $(BUILD)/boot.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$(BUILD)/boot.bin of=$@ conv=notrunc status=none

run: $(BUILD)/hadeed.img
	./scripts/run-qemu.sh $(BUILD)/hadeed.img

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
