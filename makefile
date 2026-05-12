BUILD_DIR = bin
INCLUDE_DIR = include
LOOP_DEVICE := $(shell losetup -f)

.PHONY: all clean bootloader kernel bootimg

all: $(BUILD_DIR) bootloader kernel bootimg

bootimg: 
	dd if=/dev/zero of=$(BUILD_DIR)/os.img bs=1048576 count=512
	dd if=$(BUILD_DIR)/bootloader/mbr of=$(BUILD_DIR)/os.img bs=1 count=512 conv=notrunc
	parted -s $(BUILD_DIR)/os.img mkpart primary fat32 2048s "511MiB"
	parted -s $(BUILD_DIR)/os.img set 1 boot on
	sudo dd if=$(BUILD_DIR)/bootloader/boot.bin of=$(BUILD_DIR)/os.img seek=512 conv=notrunc bs=1

	sudo losetup -o 1048576 --sizelimit 503316480 $(LOOP_DEVICE) $(BUILD_DIR)/os.img
	sudo mkfs.fat -F 32 $(BUILD_DIR)/os.img --offset 2048 -h 2048 -n "CONNOROS" -s 4
	sudo mmd -i $(LOOP_DEVICE) ::/boot
	sudo mcopy -i $(LOOP_DEVICE) $(BUILD_DIR)/kernel/kernel.elf ::/boot
	sudo mcopy -s -i $(LOOP_DEVICE) sysroot/* ::/
	sudo losetup -d $(LOOP_DEVICE)

bootloader:
	make -C bootloader BUILD_DIR=$(abspath $(BUILD_DIR)) INCLUDE_DIR=$(abspath $(INCLUDE_DIR))

kernel:
	make -C kernel BUILD_DIR=$(abspath $(BUILD_DIR)) INCLUDE_DIR=$(abspath $(INCLUDE_DIR))

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR) 

clean:
	@-rm -rf $(BUILD_DIR)