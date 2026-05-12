CC := x86_64-elf-gcc
LD := x86_64-elf-gcc
NASM := nasm
CFLAGS := -ffreestanding -I src/include/ -fno-pic -mno-red-zone -z max-page-size=0x1000 -mcmodel=kernel -I $(INCLUDE_DIR) -Wall -O0 -mgeneral-regs-only
LDFLAGS := -ffreestanding -nostdlib -fno-pic -mno-red-zone -z max-page-size=0x1000 -mcmodel=kernel

SRCS := $(shell find -name '*.c')
NASMSRCS := $(shell find -name '*.asm')

OBJS := $(patsubst %.c,$(BUILD_DIR)/kernel/%.c.o,$(SRCS)) $(patsubst %.asm,$(BUILD_DIR)/kernel/%.asm.o,$(NASMSRCS))
OBJDIRS := $(sort $(dir $(patsubst %,$(BUILD_DIR)/kernel/%,$(SRCS) $(NASMSRCS))))

TARGET := $(BUILD_DIR)/kernel/kernel.elf

all: $(BUILD_DIR)/kernel $(OBJDIRS) $(TARGET)

$(OBJDIRS):
	mkdir -p $@

$(BUILD_DIR)/kernel:
	mkdir -p $(BUILD_DIR)/kernel

$(BUILD_DIR)/kernel/%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.asm.o: %.asm
	$(NASM) $< -o $@ -f elf64

$(TARGET): $(OBJS)
	objcopy -O elf64-x86-64 -B i386 -I binary font.psf font.o
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $^ font.o -lgcc 
	rm font.o

.PHONY: all