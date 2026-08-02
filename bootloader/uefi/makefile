.PHONY: all main

all: $(BUILD_DIR)/uefi main

$(BUILD_DIR)/uefi:
	mkdir -p $(BUILD_DIR)/uefi

main: main.c
	gcc main.c -c -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -I /usr/include/efi -I /usr/include/efi/x86_64 -DEFI_FUNCTION_WRAPPER -o $(BUILD_DIR)/uefi/main.o
	ld $(BUILD_DIR)/uefi/main.o /usr/lib/crt0-efi-x86_64.o -nostdlib -znocombreloc -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -L /usr/lib -l:libgnuefi.a -l:libefi.a -o $(BUILD_DIR)/uefi/main.so 
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --output-target=efi-app-x86_64 $(BUILD_DIR)/uefi/main.so $(BUILD_DIR)/uefi/bootx64.efi