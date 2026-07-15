#include <stdint.h>
#include <string.h>
#include <pci.h>
#include <common.h>
#include <ahci.h>
#include <ata.h>
#include <mm.h>
#include <disk.h>
#include <stddef.h>

int ahci_device_count = 0;
ahci_device_t** ahci_devices = NULL;

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

void ata_string_convert(char* str){
    for (int i = 0; i < 40; i += 2) {
        char temp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = temp;
    }

    for (int i = 39; i >= 0; i--) {
        if (str[i] == ' ' || str[i] == '\0') {
            str[i] = '\0';
        } else {
            break;
        }
    }
}

// Start command engine
void start_cmd(hba_port_t* port)
{
	while (port->cmd_status & HBA_PxCMD_CR);
 	port->cmd_status |= HBA_PxCMD_FRE;  // Set FIS recieve enable bit
	port->cmd_status |= HBA_PxCMD_ST;   // Set start bit
}
 
// Stop command engine
void stop_cmd(hba_port_t *port)
{
	port->cmd_status &= ~HBA_PxCMD_ST;  // Clear start bit
 	port->cmd_status &= ~HBA_PxCMD_FRE; // Clear FIS recieve enable bit
 	while(1)
	{
        // Wait for the AHCI controller to stop recieving commands
		if (port->cmd_status & HBA_PxCMD_FR)
			continue;
		if (port->cmd_status & HBA_PxCMD_CR)
			continue;
		break;
	}
 
}

void configure_port(hba_port_t* port){
    stop_cmd(port);
    // Allocate memory for the ports command list
    uint64_t addr = (uint64_t)allocate_frame();
    port->cmd_list_base = (uint32_t)addr;
    port->cmd_list_baseu = (uint32_t)(addr >> 32);
    memset(phys_to_virt((void*)addr),0,4096);
    map_page_current(phys_to_virt((void*)addr),(void*)addr,PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_CACHEDISABLE); // Map the memory as uncacheable

    // Allocate memory for the ports recieve fis
    addr = (uint64_t)allocate_frame();
    port->fis_base = (uint32_t)addr;
    port->fis_baseu = (uint32_t)(addr >> 32);
    memset(phys_to_virt((void*)addr),0,4096);
    map_page_current(phys_to_virt((void*)addr),(void*)addr,PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_CACHEDISABLE); // Map the memory as uncacheable

    hba_cmd_header_t* cmd_list = phys_to_virt((void*)((uint64_t)port->cmd_list_base | ((uint64_t)port->cmd_list_baseu << 32)));
    for(int i = 0; i < 32; i++){
        // Allocate memory for the command table of each slot in the command list
        uint64_t cmd_table_addr = (uint64_t)allocate_frame();
        map_page_current(phys_to_virt((void*)addr),(void*)addr,PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_CACHEDISABLE); // Map the memory as uncacheable
        memset(phys_to_virt((void*)cmd_table_addr),0,4096);
        cmd_list[i].ctba = (uint32_t)cmd_table_addr;
        cmd_list[i].ctbau = (uint32_t)(cmd_table_addr >> 32);
    }
    start_cmd(port);
}

ahci_device_t* ahci_get_device(int no){
    if(no > ahci_device_count) return NULL;
    return ahci_devices[no];
}

void sata_read(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    hba_port_t* port = ahci_devices[device_no]->port;
    uint64_t buffer_phys = (uint64_t)allocate_frames(((sector_count * 512) + 4095) / 4096);

    hba_cmd_header_t* cmd_list = phys_to_virt((void*)((uint64_t)port->cmd_list_base | ((uint64_t)port->cmd_list_baseu << 32)));
    cmd_list->write = 0;
    cmd_list->prdtl = 1; // 1 prdt entry
    cmd_list->cmd_fis_length = sizeof(FIS_REG_H2D) / sizeof(uint32_t);  // fis length is measured in dwords

    hba_cmd_table_t* cmd_table = phys_to_virt((void*)((uint64_t)cmd_list->ctba | ((uint64_t)cmd_list->ctbau << 32)));
    cmd_table->prdt_entry[0].dbc = (sector_count * 512) - 1;
    cmd_table->prdt_entry[0].dba = buffer_phys;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)cmd_table->cfis;
    fis->c = 1; // command
    fis->command = READ_DMA_EXT;
    uint8_t* lba_bytes = (uint8_t*)&lba;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->lba0 = lba_bytes[0];
    fis->lba1 = lba_bytes[1];
    fis->lba2 = lba_bytes[2];
    fis->lba3 = lba_bytes[3];
    fis->lba4 = lba_bytes[4];
    fis->lba5 = lba_bytes[5];
    fis->countl = (uint8_t)sector_count;
    fis->counth = (uint8_t)(sector_count >> 8);
    fis->device = 1<<6;

    while(port->task_file_data & 0x80);    // Wait for BSY bit to be cleared
    port->command_issue = 1;
    while(1){
        if(!(port->command_issue & 1)) break;   // Return once the command has finished
        if(port->int_status & (1 << 30)){   // Task File Error Status
            panic("ahci error");
        }
    } 
    memcpy(buffer,phys_to_virt((void*)buffer_phys),sector_count * 512);
    free_frames((void*)buffer_phys,(((sector_count * 512) + 4095) / 4096));
}

void sata_write(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    hba_port_t* port = ahci_devices[device_no]->port;
    uint64_t buffer_phys = (uint64_t)allocate_frames(((sector_count * 512) + 4095) / 4096);
    memcpy(phys_to_virt((void*)buffer_phys),buffer,sector_count * 512);

    hba_cmd_header_t* cmd_list = phys_to_virt((void*)((uint64_t)port->cmd_list_base | ((uint64_t)port->cmd_list_baseu << 32)));
    cmd_list->write = 1;
    cmd_list->prdtl = 1; // 1 prdt entry
    cmd_list->cmd_fis_length = sizeof(FIS_REG_H2D) / sizeof(uint32_t);  // fis length is measured in dwords

    hba_cmd_table_t* cmd_table = phys_to_virt((void*)((uint64_t)cmd_list->ctba | ((uint64_t)cmd_list->ctbau << 32)));
    cmd_table->prdt_entry[0].dbc = (sector_count * 512) - 1;
    cmd_table->prdt_entry[0].dba = buffer_phys;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)cmd_table->cfis;
    fis->c = 1; // command
    fis->command = WRITE_DMA_EXT;
    uint8_t* lba_bytes = (uint8_t*)&lba;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->lba0 = lba_bytes[0];
    fis->lba1 = lba_bytes[1];
    fis->lba2 = lba_bytes[2];
    fis->lba3 = lba_bytes[3];
    fis->lba4 = lba_bytes[4];
    fis->lba5 = lba_bytes[5];
    fis->countl = (uint8_t)sector_count;
    fis->counth = (uint8_t)(sector_count >> 8);
    fis->device = 1<<6;

    while(port->task_file_data & 0x80);    // Wait for BSY bit to be cleared
    port->command_issue = 1;
    while(1){
        if(!(port->command_issue & 1)) break;   // Return once the command has finished
        if(port->int_status & (1 << 30)){   // Task File Error Status
            panic("ahci error");
        }
    } 
    free_frames((void*)buffer_phys,(((sector_count * 512) + 4095) / 4096));
}

void sata_identify(int device_no, void* buffer){
    hba_port_t* port = ahci_devices[device_no]->port;
    uint64_t buffer_phys = (uint64_t)allocate_frame();

    hba_cmd_header_t* cmd_list = phys_to_virt((void*)((uint64_t)port->cmd_list_base | ((uint64_t)port->cmd_list_baseu << 32)));
    cmd_list->write = 0;
    cmd_list->prdtl = 1; // 1 prdt entry
    cmd_list->cmd_fis_length = sizeof(FIS_REG_H2D) / sizeof(uint32_t);  // fis length is measured in dwords

    hba_cmd_table_t* cmd_table = phys_to_virt((void*)((uint64_t)cmd_list->ctba | ((uint64_t)cmd_list->ctbau << 32)));
    cmd_table->prdt_entry[0].dbc = 511;
    cmd_table->prdt_entry[0].dba = buffer_phys;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)cmd_table->cfis;
    fis->c = 1;
    fis->command = IDENTIFY;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->lba0 = fis->lba1 = fis->lba2 = fis->lba3 = fis->lba4 = fis->lba5 = 0;
    fis->countl = 0;
    fis->counth = 0;
    fis->device = 1<<6;

    while(port->task_file_data & 0x80);    // Wait for BSY bit to be cleared
    port->command_issue = 1;
    while(1){
        if(!(port->command_issue & 1)) break;
        if(port->int_status & (1 << 30)){   // Task File Error Status
            panic("ahci error");
        }
    } 
    memcpy(buffer,phys_to_virt((void*)buffer_phys), 512);
    free_frame((void*)buffer_phys);
}

void satapi_read(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    hba_port_t* port = ahci_devices[device_no]->port;
    uint64_t buffer_phys = (uint64_t)allocate_frames(((sector_count * 2048) + 4095) / 4096);
    
    hba_cmd_header_t* cmd_list = phys_to_virt((void*)((uint64_t)port->cmd_list_base | ((uint64_t)port->cmd_list_baseu << 32)));
    cmd_list->write = 0;
    cmd_list->prdtl = 1; // 1 prdt entry
    cmd_list->cmd_fis_length = sizeof(FIS_REG_H2D) / sizeof(uint32_t);  // fis length is measured in dwords
    cmd_list->atapi = 1;

    hba_cmd_table_t* cmd_table = phys_to_virt((void*)((uint64_t)cmd_list->ctba | ((uint64_t)cmd_list->ctbau << 32)));
    cmd_table->prdt_entry[0].dbc = (sector_count * 2048) - 1;
    cmd_table->prdt_entry[0].dba = buffer_phys;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)cmd_table->cfis;
    memset(fis,0,sizeof(FIS_REG_H2D));
    fis->c = 1;
    fis->command = ATA_PACKET;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->featurel = 1;  // ATAPI Features register. Bit 0 set means DMA mode

    cmd_table->acmd[0] = ATAPI_READ;
    cmd_table->acmd[1] = 0;
    cmd_table->acmd[2] = (uint8_t)((lba >> 24)& 0xff);
    cmd_table->acmd[3] = (uint8_t)((lba >> 16)& 0xff);
    cmd_table->acmd[4] = (uint8_t)((lba >> 8)& 0xff);
    cmd_table->acmd[5] = (uint8_t)((lba >> 0)& 0xff);
    cmd_table->acmd[6] = 0;
    cmd_table->acmd[7] = 0;
    cmd_table->acmd[8] = 0;
    cmd_table->acmd[9] = (uint8_t)(sector_count & 0xff);
    cmd_table->acmd[10] = 0;
    cmd_table->acmd[11] = 0;
    cmd_table->acmd[12] = 0;
    cmd_table->acmd[13] = 0;
    cmd_table->acmd[14] = 0;
    cmd_table->acmd[15] = 0;

    while(port->task_file_data & 0x80);    // Wait for BSY bit to be cleared
    port->command_issue = 1;
    while(1){
        // TODO: Add error checking
        if(!(port->command_issue & 1)) break;
    } 
    memcpy(buffer,phys_to_virt((void*)buffer_phys),sector_count * 2048);
    free_frames((void*)buffer_phys,(((sector_count * 2048) + 4095) / 4096));
}

void ahci_enumerate_ports(hba_memory_t* abar){
    for(int i = 0; i < 32; i++){
        if(abar->ports_implemented & (1 << i)){
            hba_port_t* port = &abar->ports[i];
            if(port->signature == SATA_SIG_ATA){
                ahci_devices = realloc(ahci_devices,sizeof(ahci_device_t*) * (ahci_device_count + 1));
                ahci_device_t* device = malloc(sizeof(ahci_device_t));
                device->port = port;
                device->type = AHCI_DEVICE_SATA;
                ahci_devices[ahci_device_count] = device;
                configure_port(port);
                uint16_t* identify_info = malloc(BYTES_PER_SECTOR);
                sata_identify(ahci_device_count, identify_info);
                ata_string_convert((char*)&identify_info[ATA_IDENTITY_MODEL_WORD]);

                uint64_t lba_size = *(uint64_t*)&identify_info[ATA_IDENTITY_MAX_LBA_EXT_WORD];
                
                struct disk disk;
                disk.block_size = BYTES_PER_SECTOR;
                disk.type = DISK_HARDDRIVE;
                disk.internal_no = ahci_device_count;
                disk.read = sata_read;
                disk.write = sata_write;
                disk.lba_size = lba_size;
                strcpy(disk.disk_name,(char*)&identify_info[ATA_IDENTITY_MODEL_WORD]);
                register_disk(disk);
                
                ahci_device_count++;
            }
            if(port->signature == SATA_SIG_ATAPI){
                ahci_devices = realloc(ahci_devices,sizeof(ahci_device_t*) * (ahci_device_count + 1));
                ahci_device_t* device = malloc(sizeof(ahci_device_t));
                device->port = port;
                device->type = AHCI_DEVICE_SATAPI;
                ahci_devices[ahci_device_count] = device;
                configure_port(port);

                struct disk disk;
                disk.block_size = BYTES_PER_SECTOR_OPTICAL;
                disk.type = DISK_OPTICAL;
                disk.internal_no = ahci_device_count;
                disk.read = satapi_read;
                disk.write = NULL;
                disk.lba_size = 0;
                strcpy(disk.disk_name,"AHCI Optical Disk");
                register_disk(disk);

                ahci_device_count++;
            }
        }
    }
}

void ahci_init(uint8_t bus, uint8_t dev, uint8_t func){
    uint32_t bar5 = pci_config_read(bus,dev,func,0x24);
    
    map_page_current(phys_to_virt((void*)(uint64_t)bar5),(void*)(uint64_t)bar5, PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_CACHEDISABLE); // Map the HBA Memory as uncacheable
    hba_memory_t* abar = phys_to_virt((void*)(uint64_t)bar5);

    ahci_enumerate_ports(abar);
}