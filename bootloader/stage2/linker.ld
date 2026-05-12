ENTRY(_start)
 
SECTIONS
{

	. = 0x8000;
 
    .text               : { __text_start = .;       *(.text)    }
    .data               : { __data_start = .;       *(.data)    }
    .rodata             : { __rodata_start = .;     *(.rodata)  }
    .bss                : { __bss_start = .;        *(.bss)     }
    . = ALIGN(4096);
    end = .;
}