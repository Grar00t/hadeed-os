CC      ?= gcc
AS      ?= as
LD      ?= ld
OBJCOPY ?= objcopy
QEMU    ?= qemu-system-x86_64
TIMEOUT ?= timeout

BUILD := build
STAGE1_SECTORS := 8
STAGE1_BYTES := 4096
KERNEL_LBA := 9
IMAGE_SECTORS := 2880

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffreestanding -nostdlib \
          -fno-stack-protector -fno-pic -mno-red-zone -m64

all: $(BUILD)/hadeed.img $(BUILD)/m2_image_test $(BUILD)/m2_trace_verify

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/kernel_entry.o: kernel/entry.asm | $(BUILD)
	$(AS) --64 $< -o $@

$(BUILD)/kernel_main.o: kernel/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(BUILD)/kernel_main.o kernel/linker.ld
	$(LD) -m elf_x86_64 -nostdlib -T kernel/linker.ld -o $@ $(BUILD)/kernel_entry.o $(BUILD)/kernel_main.o

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@test $$(wc -c < $@) -gt 0
	@test $$(wc -c < $@) -le 32768

$(BUILD)/stage1.o: boot/stage1.asm $(BUILD)/kernel.bin | $(BUILD)
	@bytes=$$(wc -c < $(BUILD)/kernel.bin); \
	sectors=$$(( (bytes + 511) / 512 )); \
	test $$sectors -gt 0; test $$sectors -le 64; \
	$(AS) --64 --defsym KERNEL_SECTORS=$$sectors $< -o $@

$(BUILD)/stage1.bin: $(BUILD)/stage1.o
	$(LD) -m elf_x86_64 -nostdlib -Ttext 0x8000 --oformat binary -e stage1 -o $@ $<
	@test $$(wc -c < $@) -gt 0
	@test $$(wc -c < $@) -le $(STAGE1_BYTES)

$(BUILD)/stage1.pad: $(BUILD)/stage1.bin
	dd if=/dev/zero of=$@ bs=1 count=$(STAGE1_BYTES) status=none
	dd if=$< of=$@ conv=notrunc status=none

$(BUILD)/boot.o: boot/bootloader.asm | $(BUILD)
	$(AS) --64 $< -o $@

$(BUILD)/boot.bin: $(BUILD)/boot.o
	$(LD) -m elf_x86_64 -nostdlib -Ttext 0x7c00 --oformat binary -e start -o $@ $<
	@test $$(wc -c < $@) -eq 512
	@test "$$(tail -c 2 $@ | od -An -tx1 | tr -d ' \n')" = 55aa

$(BUILD)/kernel.pad: $(BUILD)/kernel.bin
	@bytes=$$(wc -c < $<); sectors=$$(( (bytes + 511) / 512 )); \
	count=$$(( sectors * 512 )); \
	dd if=/dev/zero of=$@ bs=1 count=$$count status=none; \
	dd if=$< of=$@ conv=notrunc status=none

$(BUILD)/hadeed.img: $(BUILD)/boot.bin $(BUILD)/stage1.pad $(BUILD)/kernel.pad
	dd if=/dev/zero of=$@ bs=512 count=$(IMAGE_SECTORS) status=none
	dd if=$(BUILD)/boot.bin of=$@ conv=notrunc status=none
	dd if=$(BUILD)/stage1.pad of=$@ bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD)/kernel.pad of=$@ bs=512 seek=$(KERNEL_LBA) conv=notrunc status=none

$(BUILD)/m2_image_test.o: tests/m2_image_test.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/m2_image_test: $(BUILD)/m2_image_test.o tests/host_linker.ld
	$(LD) -m elf_x86_64 -nostdlib -T tests/host_linker.ld -o $@ $(BUILD)/m2_image_test.o

$(BUILD)/m2_trace_verify.o: tests/m2_trace_verify.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/m2_trace_verify: $(BUILD)/m2_trace_verify.o tests/host_linker.ld
	$(LD) -m elf_x86_64 -nostdlib -T tests/host_linker.ld -o $@ $(BUILD)/m2_trace_verify.o

trace-verify: $(BUILD)/hadeed.img $(BUILD)/kernel.elf $(BUILD)/m2_trace_verify
	./$(BUILD)/m2_trace_verify

static-test: $(BUILD)/hadeed.img $(BUILD)/m2_image_test
	./$(BUILD)/m2_image_test

runtime-test: $(BUILD)/hadeed.img
	rm -f $(BUILD)/m2.debug $(BUILD)/m2.expected $(BUILD)/qemu.log
	printf 'LM64\n' > $(BUILD)/m2.expected
	@set +e; \
	$(TIMEOUT) 2s $(QEMU) \
	  -machine pc,accel=tcg \
	  -m 64M \
	  -drive format=raw,file=$(BUILD)/hadeed.img,if=ide,index=0,media=disk \
	  -boot c \
	  -display none -serial none -monitor none \
	  -debugcon file:$(BUILD)/m2.debug -global isa-debugcon.iobase=0xe9 \
	  -d int,cpu_reset -D $(BUILD)/qemu.log \
	  -no-reboot -no-shutdown; \
	rc=$$?; set -e; \
	test $$rc -eq 124
	cmp $(BUILD)/m2.expected $(BUILD)/m2.debug
	! grep -Eiq 'triple fault' $(BUILD)/qemu.log
	printf '%s\n' 'M2_RUNTIME_PASS'

check: static-test runtime-test

run: $(BUILD)/hadeed.img
	./scripts/run-qemu.sh $(BUILD)/hadeed.img

clean:
	rm -rf $(BUILD)

.PHONY: all trace-verify static-test runtime-test check run clean
