.PHONY: all

all: $(BUILD_DIR)/bootloader stage1 stage2

$(BUILD_DIR)/bootloader:
	mkdir -p $(BUILD_DIR)/bootloader

stage1: $(BUILD_DIR)/bootloader/mbr

$(BUILD_DIR)/bootloader/mbr: stage1/mbr.asm
	nasm -f bin stage1/mbr.asm -o $(BUILD_DIR)/bootloader/mbr

stage2: $(BUILD_DIR)/bootloader/boot.bin

SRCS := $(shell find stage2 -name '*.c')
NASMSRCS := $(shell find stage2 -name '*.asm')

OBJS := $(patsubst stage2/%.asm,$(BUILD_DIR)/bootloader/%.asm.o,$(NASMSRCS)) $(patsubst stage2/%.c,$(BUILD_DIR)/bootloader/%.c.o,$(SRCS))

$(BUILD_DIR)/bootloader/%.c.o: stage2/%.c
	x86_64-elf-gcc -ffreestanding -mno-red-zone -z max-page-size=0x1000 -mcmodel=large -c $< -o $@ -I $(INCLUDE_DIR) -O2

$(BUILD_DIR)/bootloader/%.asm.o: stage2/%.asm
	nasm $< -o $@ -f elf64

$(BUILD_DIR)/bootloader/boot.bin: $(OBJS)
	x86_64-elf-gcc -ffreestanding -nostdlib -mno-red-zone -mcmodel=large -T stage2/linker.ld -o $(BUILD_DIR)/bootloader/boot.bin.elf $^ -lgcc -O2
	objcopy -O binary $(BUILD_DIR)/bootloader/boot.bin.elf $(BUILD_DIR)/bootloader/boot.bin
