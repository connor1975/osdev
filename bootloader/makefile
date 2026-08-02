.PHONY: all bios uefi

ifeq ($(wildcard /usr/lib/libgnuefi.a),)
HAVE_GNUEFI := 0
else
HAVE_GNUEFI := 1
endif

all: bios uefi

bios:
	$(MAKE) -C bios

ifeq ($(HAVE_GNUEFI),1)

uefi:
	$(MAKE) -C uefi

else

uefi:
	@echo "GNU-EFI not found, skipping UEFI bootloader."

endif