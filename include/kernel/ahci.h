#ifndef AHCI_H
#define AHCI_H

#include <kernel/ata.h>

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

typedef enum{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

typedef struct tagFIS_PIO_SETUP
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  rsv1:1;
 
	uint8_t  status;		// Status register
	uint8_t  error;		// Error register
 
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
 
	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  rsv2;		// Reserved
 
	// DWORD 3
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  rsv3;		// Reserved
	uint8_t  e_status;	// New value of status register
 
	// DWORD 4
	uint16_t tc;		// Transfer count
	uint8_t  rsv4[2];	// Reserved
} FIS_PIO_SETUP;

typedef struct hba_cmd_header
{
	// DW0
	uint8_t  cmd_fis_length:5;		// Command FIS length in DWORDS, 2 ~ 16
	uint8_t  atapi:1;		// ATAPI
	uint8_t  write:1;		// Write, 1: H2D, 0: D2H
	uint8_t  prefetchable:1;		// Prefetchable
 
	uint8_t  reset:1;		// Reset
	uint8_t  bist:1;		// BIST
	uint8_t  c:1;		// Clear busy upon R_OK
	uint8_t  rsv0:1;		// Reserved
	uint8_t  pmp:4;		// Port multiplier port
 
	uint16_t prdtl;		// Physical region descriptor table length in entries
 
	// DW1
	volatile
	uint32_t prdbc;		// Physical region descriptor byte count transferred
 
	// DW2, 3
	uint32_t ctba;		// Command table descriptor base address
	uint32_t ctbau;		// Command table descriptor base address upper 32 bits
 
	// DW4 - 7
	uint32_t rsv1[4];	// Reserved
} hba_cmd_header_t;

typedef struct hba_prdt_entry
{
	uint32_t dba;		// Data base address
	uint32_t dbau;		// Data base address upper 32 bits
	uint32_t rsv0;		// Reserved
 
	// DW3
	uint32_t dbc:22;		// Byte count, 4M max
	uint32_t rsv1:9;		// Reserved
	uint32_t i:1;		// Interrupt on completion
} hba_prdt_entry_t;

typedef struct hba_cmd_table
{
	// 0x00
	uint8_t  cfis[64];	// Command FIS
 
	// 0x40
	uint8_t  acmd[16];	// ATAPI command, 12 or 16 bytes
 
	// 0x50
	uint8_t  rsv[48];	// Reserved
 
	// 0x80
	hba_prdt_entry_t prdt_entry[1];	// Physical region descriptor table entries, 0 ~ 65535
} hba_cmd_table_t;

typedef struct tagFIS_REG_H2D
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_REG_H2D
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:3;		// Reserved
	uint8_t  c:1;		// 1: Command, 0: Control
 
	uint8_t  command;	// Command register
	uint8_t  featurel;	// Feature register, 7:0
 
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
 
	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  featureh;	// Feature register, 15:8
 
	// DWORD 3
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  icc;		// Isochronous command completion
	uint8_t  control;	// Control register
 
	// DWORD 4
	uint8_t  rsv1[4];	// Reserved
} FIS_REG_H2D;

typedef volatile struct hba_port
{
	uint32_t cmd_list_base;		// 0x00, command list base address, 1K-byte aligned
	uint32_t cmd_list_baseu;		// 0x04, command list base address upper 32 bits
	uint32_t fis_base;		// 0x08, FIS base address, 256-byte aligned
	uint32_t fis_baseu;		// 0x0C, FIS base address upper 32 bits
	uint32_t int_status;		// 0x10, interrupt status
	uint32_t int_enable;		// 0x14, interrupt enable
	uint32_t cmd_status;		// 0x18, command and status
	uint32_t reserved0;		// 0x1C, Reserved
	uint32_t task_file_data;		// 0x20, task file data
	uint32_t signature;		// 0x24, signature
	uint32_t sata_status;		// 0x28, SATA status (SCR0:SStatus)
	uint32_t sata_control;		// 0x2C, SATA control (SCR2:SControl)
	uint32_t sata_error;		// 0x30, SATA error (SCR1:SError)
	uint32_t sata_active;		// 0x34, SATA active (SCR3:SActive)
	uint32_t command_issue;		// 0x38, command issue
	uint32_t sata_notification;		// 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fis_based_switching_ctrl;		// 0x40, FIS-based switch control
	uint32_t reserved1[11];	// 0x44 ~ 0x6F, Reserved
	uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} hba_port_t;

typedef volatile struct hba_memory{
    uint32_t host_capabilities;
    uint32_t global_host_control;
    uint32_t int_status;
    uint32_t ports_implemented;
    uint32_t version;
    uint32_t ccc_ctrl;
    uint32_t ccc_ports;
    uint32_t enc_mgmt_loc;
    uint32_t enc_mgmt_ctrl;
    uint32_t host_capabilities_ext;
    uint32_t bios_os_handoff_ctrl;

    uint8_t reserved[0x74];
    uint8_t vendor_regs[0x60];

    hba_port_t ports[];
} hba_memory_t;

enum {
    AHCI_DEVICE_SATA,
    AHCI_DEVICE_SATAPI
};

typedef struct ahci_device{
    int type;
    hba_port_t* port;
}ahci_device_t;

void sata_read(int device_no, uint64_t lba, uint16_t sector_count, void* buffer);
void sata_write(int device_no, uint64_t lba, uint16_t sector_count, void* buffer);
void satapi_read(int device_no, uint32_t lba, uint16_t sector_count,void* buffer);
void sata_identify(int device_no, void* buffer);
void ahci_init(uint8_t bus, uint8_t dev, uint8_t func);
ahci_device_t* ahci_get_device(int no);

#endif