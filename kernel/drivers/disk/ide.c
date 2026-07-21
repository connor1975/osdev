#include <ata.h>
#include <stdint.h>
#include <stdio.h>
#include <pci.h>
#include <io.h>
#include <heap.h>
#include <mm.h>
#include <disk.h>
#include <string.h>
#include <debug.h>

// todo - atapi

enum ide_drive_type{
    IDE_DRIVE_NONE,
    IDE_DRIVE_ATA,
    IDE_DRIVE_ATAPI
};

struct ide_drive{
    enum ide_drive_type type;
    int supports_lba48;
    int supports_dma;
    struct ide_channel* channel;
    uint64_t lba_size;
    int slave;
    char name[64];
    int sata;
};

__attribute__((packed)) struct prdt_entry {
    uint32_t base_addr;
    uint16_t byte_count;
    uint16_t reserved:15;
    uint16_t end_of_table:1;
};

struct ide_channel {
    int secondary;
    uint32_t cmd_base;
    uint32_t ctrl_reg;
    uint32_t bm_ide;
    struct prdt_entry* prdt;
    uint32_t prdt_phys;
    struct ide_drive drives[2];
};

static int ide_drive_count = 0;
static struct ide_drive* ide_drives[4];

static struct ide_channel channels[2];

static inline uint8_t ide_delay(struct ide_channel* channel){
    for(int i = 0; i < 14; i++){
        inb(channel->ctrl_reg);
    }
    return inb(channel->ctrl_reg);
}

static inline void ide_io_delay(struct ide_channel* channel){
    inb(channel->ctrl_reg);
}

int ide_poll(struct ide_channel* channel, int pio) {
    ide_delay(channel);

    uint8_t status;
    while (inb(channel->cmd_base + ATA_STATUS_REG) & ATA_SR_BSY);

    while (pio) {
        status = inb(channel->cmd_base + ATA_STATUS_REG);
        if (status & (ATA_SR_ERR | ATA_SR_DF))
            return 1;
        if (status & ATA_SR_DRQ)
            break;
    }
    status = inb(channel->cmd_base + ATA_STATUS_REG);
    if (status & ATA_SR_ERR || status & ATA_SR_DF)
        return 1;

   return 0;
}

void ide_channel_setup_prdt(struct ide_channel* channel, int byte_count){
    int entries = (byte_count + (PAGE_SIZE - 1)) / PAGE_SIZE;
    struct prdt_entry* entry;
    for(int i = 0; i < entries - 1; i++){
        entry = &channel->prdt[i];
        entry->base_addr = (uint32_t)(uint64_t)allocate_frame();
        entry->reserved = 0;
        entry->byte_count = PAGE_SIZE;
        entry->end_of_table = 0;
    }
    entry = &channel->prdt[entries - 1];
    entry->end_of_table = 1;
    entry->reserved = 0;
    entry->base_addr = (uint32_t)(uint64_t)allocate_frame();
    if(byte_count % 4096){
        entry->byte_count = byte_count % 4096;
    }else{
        entry->byte_count = 4096;
    }
}

void ide_free_dma_memory(struct ide_channel* channel){
    struct prdt_entry* entry = channel->prdt;
    while(!entry->end_of_table){
        free_frame((void*)(uint64_t)entry->base_addr);
        entry++;
    }
    free_frame((void*)(uint64_t)entry->base_addr);
}

void ide_channel_access_prdt_memory(struct ide_channel* channel, int byte_count, void* buffer, int write){
    int entries = (byte_count + (PAGE_SIZE - 1)) / PAGE_SIZE;
    struct prdt_entry* entry = &channel->prdt[0];
    void* buffer_ptr = buffer;
    for(int i = 0; i < entries - 1; i++){
        if(write)
            memcpy(phys_to_virt((void*)(uint64_t)entry->base_addr),buffer_ptr, PAGE_SIZE);
        else
            memcpy(buffer_ptr, phys_to_virt((void*)(uint64_t)entry->base_addr), PAGE_SIZE);

        buffer_ptr += PAGE_SIZE;
        entry++;
    }
    
    if(byte_count % 4096){
        if(write)
            memcpy(phys_to_virt((void*)(uint64_t)entry->base_addr),buffer_ptr, byte_count % 4096);
        else
            memcpy(buffer_ptr, phys_to_virt((void*)(uint64_t)entry->base_addr), byte_count % 4096);
    }else{
        if(write)
            memcpy(phys_to_virt((void*)(uint64_t)entry->base_addr),buffer_ptr, PAGE_SIZE);
        else
            memcpy(buffer_ptr, phys_to_virt((void*)(uint64_t)entry->base_addr), PAGE_SIZE);
    }
}

void ide_write_dma(struct ide_channel* channel, struct ide_drive* drive, uint64_t lba, uint16_t sector_count, void* buffer){
    ide_channel_setup_prdt(channel, sector_count * BYTES_PER_SECTOR);
    ide_channel_access_prdt_memory(channel, sector_count * BYTES_PER_SECTOR, buffer,1);

    ide_poll(channel,0);

    outl(channel->bm_ide + 4, channel->prdt_phys);
    outb(channel->bm_ide + BM_IDE_STATUS_REG,0x6);
    outb(channel->bm_ide,0);

    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_DRIVE_REG, 0x40 | (drive->slave << 4));
    ide_delay(channel);

    if(drive->supports_lba48){
        outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count >> 8));
        outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)((lba >> 24) & 0xff));
        outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 32) & 0xff));
        outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 40) & 0xff));
        outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count & 0xff));
        outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xff));
        outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xff));
        outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xff));
        ide_delay(channel);
        outb(channel->cmd_base + ATA_COMMAND_REG, WRITE_DMA_EXT);
    }else{
        outb(channel->cmd_base + ATA_SECCOUNT_REG, sector_count);
        outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xFF));
        outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xFF));
        outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xFF));
        ide_delay(channel);
        outb(channel->cmd_base + ATA_COMMAND_REG, WRITE_DMA);
    }

    outb(channel->bm_ide, 0x1);
    if(ide_poll(channel,1)){
        kprintf(KPRINTF_ERROR,"ide: error writing %d sectors to lba %llu on ide disk %d:%d\n",sector_count,lba,channel->secondary,drive->slave);
        return;
    }

    while(!(inb(channel->bm_ide + BM_IDE_STATUS_REG) & 0x04));
    
    outb(channel->bm_ide, 0); // stop engine
    outb(channel->bm_ide + BM_IDE_STATUS_REG, 0x04); // clear interrupt bit

    ide_free_dma_memory(channel);

    ide_poll(channel, 0);
    if(drive->supports_lba48)
        outb(channel->cmd_base + ATA_COMMAND_REG, CACHE_FLUSH_EXT);
    else
        outb(channel->cmd_base + ATA_COMMAND_REG, CACHE_FLUSH);

    ide_poll(channel,0);
}

void ide_read_dma(struct ide_channel* channel, struct ide_drive* drive, uint64_t lba, uint16_t sector_count, void* buffer){
    ide_channel_setup_prdt(channel, sector_count * BYTES_PER_SECTOR);

    ide_poll(channel,0);

    outl(channel->bm_ide + 4, channel->prdt_phys);
    outb(channel->bm_ide + BM_IDE_STATUS_REG,0x6);
    outb(channel->bm_ide,8);

    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_DRIVE_REG, 0x40 | (drive->slave << 4));
    ide_delay(channel);

    if(drive->supports_lba48){
        outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count >> 8));
        outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)((lba >> 24) & 0xff));
        outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 32) & 0xff));
        outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 40) & 0xff));
        outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count & 0xff));
        outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xff));
        outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xff));
        outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xff));
        ide_delay(channel);
        outb(channel->cmd_base + ATA_COMMAND_REG, READ_DMA_EXT);
    }else{
        outb(channel->cmd_base + ATA_SECCOUNT_REG, sector_count);
        outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xFF));
        outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xFF));
        outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xFF));
        ide_delay(channel);
        outb(channel->cmd_base + ATA_COMMAND_REG, READ_DMA);
    }

    outb(channel->bm_ide, 0x8 | 0x1);
    if(ide_poll(channel,1)){
        kprintf(KPRINTF_ERROR,"ide: error reading %d sectors from lba %llu on ide disk %d:%d\n",sector_count,lba,channel->secondary,drive->slave);
        return;
    }

    while(!(inb(channel->bm_ide + BM_IDE_STATUS_REG) & 0x04));
    
    outb(channel->bm_ide, 0); // stop engine
    outb(channel->bm_ide + BM_IDE_STATUS_REG, 0x04); // clear interrupt bit

    ide_channel_access_prdt_memory(channel, sector_count * BYTES_PER_SECTOR, buffer,0);
    ide_free_dma_memory(channel);
}

// 1 - error - possibly atapi device
// 2 - no device
int ata_identify(struct ide_channel* channel, int slave, void* buffer){ 
    outb(channel->cmd_base + ATA_DRIVE_REG, 0xA0 | (slave << 4));
    ide_delay(channel);
    outb(channel->cmd_base + ATA_SECCOUNT_REG, 0);
    outb(channel->cmd_base + ATA_LBA_LOW_REG, 0);
    outb(channel->cmd_base + ATA_LBA_MID_REG, 0);
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, 0);
    outb(channel->cmd_base + ATA_COMMAND_REG, IDENTIFY);
    ide_delay(channel);
    
    if(inb(channel->cmd_base + ATA_STATUS_REG) == 0){
        return 2;
    }

    uint16_t* bufferptr = buffer;

    if(ide_poll(channel, 1)){
        return 1;
    }
    
    for(int x = 0; x < 256; x++){
        bufferptr[x] = inw(channel->cmd_base + ATA_DATA_REG);
    }
    bufferptr+=256;
    return 0;
}

void ide_read_pio48(struct ide_channel* channel, int slave, uint64_t lba, uint16_t sector_count, void* buffer){
    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_DRIVE_REG, 0x40 | (slave << 4));
    ide_delay(channel);
    outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count >> 8));
    outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)((lba >> 24) & 0xff));
    outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 32) & 0xff));
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 40) & 0xff));
    outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count & 0xff));
    outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xff));
    outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xff));
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xff));
    outb(channel->cmd_base + ATA_COMMAND_REG, READ_SECTORS_PIO_EXT);

    ide_delay(channel);
    
    uint16_t* bufferptr = buffer;
    for(int i = 0; i < sector_count; i++){
        if(ide_poll(channel, 1)){
            kprintf(KPRINTF_ERROR,"ide: error reading %d sectors from lba %llu on ide disk %d:%d\n",sector_count,lba,channel->secondary,slave);
            return;
        }
        for(int j = 0; j < 256; j++){
            bufferptr[j] = inw(channel->cmd_base + ATA_DATA_REG);
        }
        bufferptr+=256;
    }
}

void ide_read_pio28(struct ide_channel* channel, int slave, uint32_t lba, uint16_t sector_count, void* buffer){
    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_DRIVE_REG, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
    ide_delay(channel);
    outb(channel->cmd_base + ATA_SECCOUNT_REG, sector_count);
    outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xFF));
    outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xFF));
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xFF));
    outb(channel->cmd_base + ATA_COMMAND_REG, READ_SECTORS_PIO);

    ide_delay(channel);
    
    uint16_t* bufferptr = buffer;
    for(int i = 0; i < sector_count; i++){
        if(ide_poll(channel, 1)){
            kprintf(KPRINTF_ERROR,"ide: error reading %d sectors from lba %llu on ide disk %d:%d\n",sector_count,lba,channel->secondary,slave);
            return;
        }
        for(int j = 0; j < 256; j++){
            bufferptr[j] = inw(channel->cmd_base + ATA_DATA_REG);
        }
        bufferptr+=256;
    }
}

void ide_write_pio28(struct ide_channel* channel, int slave, uint32_t lba, uint16_t sector_count, void* buffer){
    ide_poll(channel, 0);
    ide_delay(channel);
    outb(channel->cmd_base + ATA_DRIVE_REG, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
    ide_delay(channel);
    outb(channel->cmd_base + ATA_SECCOUNT_REG, (unsigned char)sector_count);
    outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)lba);
    outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)(lba >> 8));
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)(lba >> 16));
    outb(channel->cmd_base + ATA_COMMAND_REG, WRITE_SECTORS_PIO);
    ide_delay(channel);

    uint16_t* bufferptr = buffer;
    for(int i = 0; i < sector_count; i++){
        if(ide_poll(channel, 1)){
            kprintf(KPRINTF_ERROR,"ide: error writing %d sectors to lba %llu on ide disk %d:%d\n",sector_count,lba,channel->secondary,slave);
            return;
        }
        for(int x = 0; x < 256; x++){
            outw(channel->cmd_base + ATA_DATA_REG, bufferptr[x]);
        }
        bufferptr+=256;
    }
    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_COMMAND_REG, CACHE_FLUSH);
    ide_poll(channel, 0);
}

void ide_write_pio48(struct ide_channel* channel, int slave, uint64_t lba, uint16_t sector_count, void* buffer){
    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_DRIVE_REG, 0x40 | (slave << 4));
    ide_delay(channel);
    outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count >> 8));
    outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)((lba >> 24) & 0xff));
    outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 32) & 0xff));
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 40) & 0xff));
    outb(channel->cmd_base + ATA_SECCOUNT_REG, (uint8_t)(sector_count & 0xff));
    outb(channel->cmd_base + ATA_LBA_LOW_REG, (uint8_t)(lba & 0xff));
    outb(channel->cmd_base + ATA_LBA_MID_REG, (uint8_t)((lba >> 8) & 0xff));
    outb(channel->cmd_base + ATA_LBA_HIGH_REG, (uint8_t)((lba >> 16) & 0xff));
    outb(channel->cmd_base + ATA_COMMAND_REG, WRITE_SECTORS_PIO_EXT);

    ide_delay(channel);
    
    uint16_t* bufferptr = buffer;
    for(int i = 0; i < sector_count; i++){
        if(ide_poll(channel, 1)){
            kprintf(KPRINTF_ERROR,"ide: error writing %d sectors to lba %llu on ide disk %d:%d\n",sector_count,lba,channel->secondary,slave);
            return;
        }
        for(int x = 0; x < 256; x++){
            outw(channel->cmd_base + ATA_DATA_REG, bufferptr[x]);
        }
        bufferptr+=256;
    }
    ide_poll(channel, 0);
    outb(channel->cmd_base + ATA_COMMAND_REG, CACHE_FLUSH_EXT);
    ide_poll(channel, 0);
}

void ide_read(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    struct ide_drive* drive = ide_drives[device_no];
    struct ide_channel* channel = drive->channel;
    if(drive->supports_dma){
        ide_read_dma(channel,drive,lba,sector_count,buffer);
    }else{
        if(drive->supports_lba48){
            ide_read_pio48(channel,drive->slave,lba,sector_count,buffer);

        }else{
            ide_read_pio28(channel,drive->slave,lba,sector_count,buffer);
        }
    }
}

void ide_write(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    struct ide_drive* drive = ide_drives[device_no];
    struct ide_channel* channel = drive->channel;
    if(drive->supports_dma){
        ide_write_dma(channel,drive,lba,sector_count,buffer);
    }else{
        if(drive->supports_lba48){
            ide_write_pio48(channel,drive->slave,lba,sector_count,buffer);

        }else{
            ide_write_pio28(channel,drive->slave,lba,sector_count,buffer);
        }
    }
}

void ide_init(uint8_t bus, uint8_t dev, uint8_t func){
    uint8_t prog_if = pci_get_progif(bus,dev,func);
    
    pci_enable_bus_mastering(bus,dev,func);

    if(prog_if & 1){
        pci_bar_t bar0 = pci_read_bar(bus,dev,func,0);
        pci_bar_t bar1 = pci_read_bar(bus,dev,func,1);
        channels[0].cmd_base = bar0.address;
        channels[0].ctrl_reg = bar1.address + 2;
    }else{
        channels[0].cmd_base = 0x1f0;
        channels[0].ctrl_reg = 0x3f6;
    }

    if(prog_if & (1 << 2)){
        pci_bar_t bar2 = pci_read_bar(bus,dev,func,2);
        pci_bar_t bar3 = pci_read_bar(bus,dev,func,3);
        channels[1].cmd_base = bar2.address;
        channels[1].ctrl_reg = bar3.address + 2;
    }else{
        channels[1].cmd_base = 0x170;
        channels[1].ctrl_reg = 0x376;
    }

    pci_bar_t bar4 = pci_read_bar(bus,dev,func,4);
    channels[0].bm_ide = bar4.address;
    channels[1].bm_ide = bar4.address + 8;
    
    kprintf(KPRINTF_INFO,"ide: initialising pci ide controller using ports %x %x, %x %x, %x\n",channels[0].cmd_base,channels[0].ctrl_reg,channels[1].cmd_base,channels[1].ctrl_reg,channels[0].bm_ide);

    uint16_t* identity = malloc(BYTES_PER_SECTOR);
    for (int i = 0; i < 2; i++) {
        struct ide_channel* channel = &channels[i];
        channel->secondary = i;

        uint32_t prdt_phys = (uint32_t)(uint64_t)allocate_frame();
        channel->prdt_phys = prdt_phys;
        channel->prdt = phys_to_virt((void*)(uint64_t)prdt_phys);
        outl(channel->bm_ide + 4,prdt_phys);

        for (int j = 0; j < 2; j++) {
            struct ide_drive* drive = &channel->drives[j];
            int result = ata_identify(channel, j, identity);
            if (result == 0) {
                drive->type = IDE_DRIVE_ATA;
                
                drive->supports_lba48 = 0;
                if(identity[ATA_IDENTITY_COMMAND_SET_SUPPORT_2_WORD] & ATA_IDENTITY_LBA48_SUPPORTED){
                    drive->supports_lba48 = 1;
                    drive->lba_size = *(uint64_t*)&identity[ATA_IDENTITY_MAX_LBA_EXT_WORD];
                }else{
                    drive->lba_size = *(uint32_t*)&identity[ATA_IDENTITY_MAX_LBA_WORD];
                }

                drive->supports_dma = identity[ATA_IDENTITY_CAPABILITIES_WORD] & ATA_IDENTITY_DMA_SUPPORTED;

                if (identity[ATA_IDENTITY_SATA_CAPABILITIES_WORD] != 0x0000 && identity[ATA_IDENTITY_SATA_CAPABILITIES_WORD] != 0xFFFF){
                    drive->sata = 1;
                    drive->supports_dma = 0;
                }else{
                    drive->sata = 0;
                }

                drive->channel = channel;
                drive->slave = j;
                ide_drives[ide_drive_count] = drive;
                ata_string_convert((char*)&identity[ATA_IDENTITY_MODEL_WORD]);
                strcpy(drive->name,(char*)&identity[ATA_IDENTITY_MODEL_WORD]);

                struct disk disk_info;
                disk_info.internal_no = ide_drive_count++;
                disk_info.type = DISK_HARDDRIVE;
                disk_info.block_size = BYTES_PER_SECTOR;
                disk_info.read = ide_read;
                disk_info.write = ide_write;
                disk_info.lba_size = drive->lba_size;
                strcpy(disk_info.disk_name,drive->name);

                kprintf(KPRINTF_INFO,"ide: found ide hdd at %d:%d - %s - supports lba48?: %d - using dma?: %d - size in mb: %llu\n",i,j,drive->name, (drive->supports_lba48 != 0), (drive->supports_dma != 0), ((drive->lba_size * 512) / 1024) / 1024);
                if(drive->sata)
                    kprintf(KPRINTF_WARNING,"ide: drive is SATA, disabling dma to ensure compatibility\n");

                register_disk(disk_info);
            }else if (result == 1) {
                // identify error - possibly atapi
            } else if (result == 2) {
                drive->type = IDE_DRIVE_NONE;
            }
        }
    }
    free(identity);
}