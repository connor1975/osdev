KERNEL_OFF = 0xffffffff80000000;
KERNEL_VMA = KERNEL_OFF + 0x100000;
ENTRY (_start)

SECTIONS
{
    . = KERNEL_VMA;

	.text ALIGN (4K) : AT (ADDR (.text) - KERNEL_OFF)
	{
        . = ALIGN(4096);
		*(.text)
	}
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - KERNEL_OFF)
	{
		*(.rodata)
        . = ALIGN(4096);
	}
	.data ALIGN (4K) : AT (ADDR (.data) - KERNEL_OFF)
	{
		*(.data)
        . = ALIGN(4096);
	}
	.bss ALIGN (4K) : AT (ADDR (.bss) - KERNEL_OFF)
	{
		*(COMMON)
		*(.bss)
        . = ALIGN(4096);
	}

}