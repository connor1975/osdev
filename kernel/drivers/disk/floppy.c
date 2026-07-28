#include <stdint.h>
#include <io.h>
#include <time.h>
#include <debug.h>
#include <multitasking.h>
#include <interrupts.h>
#include <heap.h>
#include <disk.h>
#include <string.h>
#include <mm.h>

/*
    first iteration of floppy driver
    this driver feels hacky
    todo:   replace horrible irq polling with some kind of task sleep queue
            retries for failed commands and better error handling
            detect media changes
*/

#define FLOPPY_MAX_DRIVES 2

#define STATUS_REGISTER_A 0x3f0
#define STATUS_REGISTER_B 0x3f1
#define DIGITAL_OUTPUT_REGISTER 0x3f2
#define MAIN_STATUS_REGISTER 0x3f4
#define DATA_FIFO 0x3f5
#define CONFIGURATION_CONTROL_REGISTER 0x3f7
#define DIGITAL_INPUT_REGISTER 0x3F7

#define MSR_DIO 0x40
#define MSR_RQM 0x80
#define MSR_CB 0x10

#define DOR_MOTOR_D 0x80
#define DOR_MOTOR_C 0x40
#define DOR_MOTOR_B 0x20
#define DOR_MOTOR_A 0x10
#define DOR_IRQ_DMA 0x8
#define DOR_RESET 0x4

#define DIR_DISK_CHANGE 0x80

#define MFM_BIT 0x40

#define READ_TRACK 2
#define SPECIFY 3
#define SENSE_DRIVE_STATUS 4
#define WRITE_DATA 5
#define READ_DATA 6
#define RECALIBRATE 7
#define SENSE_INTERRUPT 8
#define DUMPREG 14
#define SEEK 15
#define VERSION 16
#define CONFIGURE 19
#define LOCK 20
#define VERIFY 22

#define FDC_DATA_RATE_500KBPS 0
#define FDC_DATA_RATE_300KBPS 1
#define FDC_DATA_RATE_250KBPS 2
#define FDC_DATA_RATE_1MBPS 3

/*
step rate: 3 ms
head unload time - 240 ms
head load time - 2 ms
non dma - 0
*/

#define FDC_SRT_HUT 0xDF 
#define FDC_HLT_NDMA 0x02

#define CMOS_FLOPPY_TYPE_NONE 0x0
#define CMOS_FLOPPY_TYPE_360 0x1
#define CMOS_FLOPPY_TYPE_120 0x2
#define CMOS_FLOPPY_TYPE_144 0x4
#define CMOS_FLOPPY_TYPE_288 0x6

#define FIFO_TIMEOUT 100
#define FDC_RESET_TIMEOUT 200

#define FLOPPY_TYPE_NONE 0
#define FLOPPY_TYPE_144 1

#define FLOPPY_MOTOR_SPINUP_WAIT 300
#define FLOPPY_MOTOR_OFF_WAIT 3000

#define FLOPPY_LONG_TIMEOUT 3000

#define FLOPPY_MOTOR_OFF 0
#define FLOPPY_MOTOR_IDLE 1
#define FLOPPY_MOTOR_ON 2

#define FLOPPY_SECTORS_PER_TRACK 18

// isa dma defines

#define FLOPPY_DMA_CHANNEL 2
#define FLOPPY_MAX_TRANSFER 0x4800

#define CHANNEL2_START_ADDR_REG 0x04
#define CHANNEL2_PAGE_ADDR_REG 0x81
#define CHANNEL2_COUNT_REG 0x05

#define FLIP_FLOP_RESET_REG 0x0C
#define DMA_SINGLE_CHANNEL_MASK_REG 0x0A

#define DMA_MODE_TO_RAM    0x04
#define DMA_MODE_FROM_RAM  0x08
#define DMA_MODE_REG 0x0B
#define DMA_MASK_SET 0x04
#define DMA_MODE_TYPE_SINGLE 0x40

/* 
    statically allocate a dma buffer for floppy disks
    isa dma is very picky, cant cross 64kb region, under 16mb
    this buffer should fit those criteria provided the kernel
    never changes from being loaded at 0x100000 phys
*/
static uint8_t floppy_dma_buffer[FLOPPY_MAX_TRANSFER] __attribute__((aligned(0x8000)));

struct floppy_drive{
    int type;
    volatile int motor_state;
    uint64_t motor_off_time;
    int disk_inserted;
    int cached_cyl;
    int cached_head;
    uint8_t* track_buffer;
};

static uint8_t dor_state;
static uint8_t controller_version;
static struct floppy_drive drives[2];

static volatile int irq_received = 0;

void floppy_irq_handler(){
    irq_received = 1;
}

void fdc_dma_setup_transfer(uint16_t length, int write){
    uint64_t phys_addr = (uint64_t)get_physical_address(floppy_dma_buffer,current_pml4);

    uint8_t direction;
    if(write){
        direction = DMA_MODE_FROM_RAM;
    }else{
        direction = DMA_MODE_TO_RAM;
    }

    outb(DMA_SINGLE_CHANNEL_MASK_REG, DMA_MASK_SET | FLOPPY_DMA_CHANNEL);
    outb(FLIP_FLOP_RESET_REG, 0x00);
    outb(DMA_MODE_REG, DMA_MODE_TYPE_SINGLE | direction | FLOPPY_DMA_CHANNEL);
    outb(CHANNEL2_START_ADDR_REG, (uint8_t)(phys_addr & 0xFF));
    outb(CHANNEL2_START_ADDR_REG, (uint8_t)((phys_addr >> 8) & 0xFF));
    outb(CHANNEL2_PAGE_ADDR_REG, (uint8_t)((phys_addr >> 16) & 0xFF));
    outb(CHANNEL2_COUNT_REG, (uint8_t)((length - 1) & 0xFF));
    outb(CHANNEL2_COUNT_REG, (uint8_t)(((length - 1) >> 8) & 0xFF));
    outb(DMA_SINGLE_CHANNEL_MASK_REG, FLOPPY_DMA_CHANNEL);
}

inline int fdc_get_msr_dio(){
    return (inb(MAIN_STATUS_REGISTER) & MSR_DIO);
}

inline int fdc_get_msr_rqm(){
    return (inb(MAIN_STATUS_REGISTER) & MSR_RQM);
}

void fdc_send_byte(uint8_t byte){
    uint64_t timeout = get_ticks_since_boot() + FIFO_TIMEOUT;
    while(get_ticks_since_boot() < timeout){
        if(fdc_get_msr_rqm() && !fdc_get_msr_dio()){
            outb(DATA_FIFO,byte);
            return;
        }
    }

    kprintf(KPRINTF_ERROR,"fdc: sending byte to controller timed out\n");
}

uint8_t fdc_read_byte(){
    uint64_t timeout = get_ticks_since_boot() + FIFO_TIMEOUT;
    while(get_ticks_since_boot() < timeout){
        if(fdc_get_msr_rqm() && fdc_get_msr_dio()){
            return inb(DATA_FIFO);
        }
    }

    kprintf(KPRINTF_ERROR,"fdc: reading byte from controller timed out\n");
    return 0;
}

uint8_t fdc_get_version(){
    fdc_send_byte(VERSION);
    return fdc_read_byte();
}

void fdc_set_data_rate(uint8_t data_rate){
    outb(CONFIGURATION_CONTROL_REGISTER,data_rate);
}

void fdc_write_dor(uint8_t byte){
    dor_state = byte;
    outb(DIGITAL_OUTPUT_REGISTER,dor_state);
}

void fdc_select_drive(int drive_num){
    uint8_t val = dor_state & (uint8_t)~3;
    fdc_write_dor(val | drive_num);
}

void fdc_motor_on(int drive_num){
    if(drives[drive_num].motor_state == FLOPPY_MOTOR_ON) 
        return;
    if(drives[drive_num].motor_state == FLOPPY_MOTOR_IDLE){
        drives[drive_num].motor_state = FLOPPY_MOTOR_ON;
        return;
    }

    uint8_t val = dor_state | (0x10 << drive_num);
    fdc_write_dor(val);
    sleep(FLOPPY_MOTOR_SPINUP_WAIT);
    drives[drive_num].motor_state = FLOPPY_MOTOR_ON;
}

void fdc_power_down_motor(int drive_num){
    uint8_t val = dor_state & ~((uint8_t)0x10 << drive_num);
    fdc_write_dor(val);
    drives[drive_num].motor_state = FLOPPY_MOTOR_OFF;
}

void fdc_motor_off(int drive_num){
    drives[drive_num].motor_state = FLOPPY_MOTOR_IDLE;
    drives[drive_num].motor_off_time = get_ticks_since_boot();
}

void fdc_sense_interrupt(uint8_t* st0, uint8_t* cyl){
    fdc_send_byte(SENSE_INTERRUPT);
    *st0 = fdc_read_byte();
    *cyl = fdc_read_byte();
}

void fdc_motor_timer(){
    while(1){
        sleep(100);

        if(drives[0].motor_state != FLOPPY_MOTOR_IDLE && drives[1].motor_state != FLOPPY_MOTOR_IDLE)
            continue;

        for(int i = 0; i < FLOPPY_MAX_DRIVES; i++){
            if(get_ticks_since_boot() >= drives[i].motor_off_time + FLOPPY_MOTOR_OFF_WAIT){
                if(drives[i].motor_state == FLOPPY_MOTOR_IDLE){
                    fdc_power_down_motor(i);
                }
            }
        }
    }
}

static inline void io_delay(){
    for(int i = 0; i < 4; i++){
        outb(0x80, 0);  // small delay using io
    }
}

int fdc_reset_controller(){
    irq_received = 0;

    outb(DIGITAL_OUTPUT_REGISTER, 0x00);
    io_delay();
    outb(DIGITAL_OUTPUT_REGISTER, DOR_RESET | DOR_IRQ_DMA);
    dor_state = DOR_RESET | DOR_IRQ_DMA;

    uint64_t reset_timeout = get_ticks_since_boot() + FDC_RESET_TIMEOUT;
    while(!irq_received){
        if(get_ticks_since_boot() > reset_timeout){
            return 1;
        }
    }

    uint8_t st0;
    uint8_t cyl;
    for(int i = 0; i < 4; i++){
        fdc_sense_interrupt(&st0,&cyl);
    }
    
    fdc_set_data_rate(FDC_DATA_RATE_500KBPS);
    
    fdc_send_byte(SPECIFY);
    fdc_send_byte(FDC_SRT_HUT);
    fdc_send_byte(FDC_HLT_NDMA);

    return 0;
}

int detect_floppy_drives(){
    int drive_count = 0;
    uint8_t floppy_drive_reg = read_cmos_register(0x10);
    uint8_t disk_b = floppy_drive_reg & 0xf;
    uint8_t disk_a = (floppy_drive_reg >> 4) & 0xf;

    switch(disk_a){
        case CMOS_FLOPPY_TYPE_144:
            drives[0].type = FLOPPY_TYPE_144;
            kprintf(KPRINTF_INFO,"fdc: detected 3.5-inch 1.44MB floppy drive\n");
            drive_count++;
            break;
        default:
            drives[0].motor_state = FLOPPY_MOTOR_OFF;
            drives[0].type = FLOPPY_TYPE_NONE;
            break;
    }
    switch(disk_b){
        case CMOS_FLOPPY_TYPE_144:
            drives[1].type = FLOPPY_TYPE_144;
            kprintf(KPRINTF_INFO,"fdc: detected 3.5-inch 1.44MB floppy drive\n");
            drive_count++;
            break;
        default:
            drives[1].motor_state = FLOPPY_MOTOR_OFF;
            drives[1].type = FLOPPY_TYPE_NONE;
            break;
    }
    return drive_count;
}

int floppy_seek(int drive_num, uint8_t cylinder, uint8_t head){
    fdc_select_drive(drive_num);
    int leave_motor_on = 0;
    if(drives[drive_num].motor_state != FLOPPY_MOTOR_ON){
        fdc_motor_on(drive_num);
        leave_motor_on = 0;

    }else{
        leave_motor_on = 1;
    }

    irq_received = 0;

    fdc_send_byte(SEEK);
    fdc_send_byte((head << 2) | drive_num);
    fdc_send_byte(cylinder);

    uint64_t timeout = get_ticks_since_boot() + FLOPPY_LONG_TIMEOUT;
    while(!irq_received){
        if(get_ticks_since_boot() > timeout){
            kprintf(KPRINTF_ERROR,"fdc: floppy drive seek timed out\n");
            if(!leave_motor_on)
                fdc_motor_off(drive_num);
            return 1;
        }
    }

    uint8_t st0;
    uint8_t cyl;
    fdc_sense_interrupt(&st0,&cyl);

    if(!leave_motor_on)
        fdc_motor_off(drive_num);
        
    if(cyl != cylinder || (st0 & 0xC0)){
        kprintf(KPRINTF_ERROR, "fdc: floppy seek error\n");
        return 1;
    }
    
    return 0;
}

int floppy_recalibrate(int drive_num){
    fdc_select_drive(drive_num);
    int leave_motor_on = 0;
    if(drives[drive_num].motor_state != FLOPPY_MOTOR_ON){
        fdc_motor_on(drive_num);
        leave_motor_on = 0;

    }else{
        leave_motor_on = 1;
    }

    irq_received = 0;

    fdc_send_byte(RECALIBRATE);
    fdc_send_byte(drive_num);

    uint64_t timeout = get_ticks_since_boot() + FLOPPY_LONG_TIMEOUT;
    while(!irq_received){
        if(get_ticks_since_boot() > timeout){
            kprintf(KPRINTF_ERROR,"fdc: floppy drive recalibrate timed out\n");
            if(!leave_motor_on)
                fdc_motor_off(drive_num);
            return 1;
        }
    }

    uint8_t st0;
    uint8_t cyl;
    fdc_sense_interrupt(&st0,&cyl);

    if(!leave_motor_on)
        fdc_motor_off(drive_num);
        
    if(cyl != 0 || (st0 & 0xC0)){
        kprintf(KPRINTF_ERROR, "fdc: floppy recalibrate error\n");
        return 1;
    }
    
    return 0;
}

int floppy_read_track(int drive_num, uint8_t cylinder, uint8_t head){
    if(drives[drive_num].cached_cyl == cylinder && drives[drive_num].cached_head == head)
        return 0;

    floppy_seek(drive_num,cylinder,head);    
    fdc_select_drive(drive_num);
    fdc_motor_on(drive_num);
    fdc_dma_setup_transfer(FLOPPY_SECTORS_PER_TRACK * BYTES_PER_SECTOR,0);
    
    irq_received = 0;
    
    fdc_send_byte(READ_DATA | MFM_BIT);
    fdc_send_byte((head << 2) | drive_num);
    fdc_send_byte(cylinder);
    fdc_send_byte(head);
    fdc_send_byte(1);
    fdc_send_byte(2);
    fdc_send_byte(18);
    fdc_send_byte(0x1b);
    fdc_send_byte(0xff);

    uint64_t timeout = get_ticks_since_boot() + FLOPPY_LONG_TIMEOUT;
    while(!irq_received){
        if(get_ticks_since_boot() > timeout){
            kprintf(KPRINTF_ERROR,"fdc: floppy drive read timed out\n");
            fdc_motor_off(drive_num);
            return 1;
        }
    }

    uint8_t st0 = fdc_read_byte();
    uint8_t st1 = fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();

    if(st0 & 0xC0 || st1 != 0){
        kprintf(KPRINTF_ERROR, "fdc: floppy read error\n");
        fdc_motor_off(drive_num);
        return 1;
    }

    memcpy(drives[drive_num].track_buffer,floppy_dma_buffer,FLOPPY_SECTORS_PER_TRACK * BYTES_PER_SECTOR);
    drives[drive_num].cached_cyl = cylinder;
    drives[drive_num].cached_head = head;

    fdc_motor_off(drive_num);
    return 0;
}

int floppy_write_track(int drive_num, uint8_t cylinder, uint8_t head){
    memcpy(floppy_dma_buffer,drives[drive_num].track_buffer,FLOPPY_SECTORS_PER_TRACK * BYTES_PER_SECTOR);

    floppy_seek(drive_num,cylinder,head);    
    fdc_select_drive(drive_num);
    fdc_motor_on(drive_num);
    fdc_dma_setup_transfer(FLOPPY_SECTORS_PER_TRACK * BYTES_PER_SECTOR,1);
    
    irq_received = 0;
    
    fdc_send_byte(WRITE_DATA | MFM_BIT);
    fdc_send_byte((head << 2) | drive_num);
    fdc_send_byte(cylinder);
    fdc_send_byte(head);
    fdc_send_byte(1);
    fdc_send_byte(2);
    fdc_send_byte(18);
    fdc_send_byte(0x1b);
    fdc_send_byte(0xff);

    uint64_t timeout = get_ticks_since_boot() + FLOPPY_LONG_TIMEOUT;
    while(!irq_received){
        if(get_ticks_since_boot() > timeout){
            kprintf(KPRINTF_ERROR,"fdc: floppy drive write timed out\n");
            fdc_motor_off(drive_num);
            return 1;
        }
    }

    uint8_t st0 = fdc_read_byte();
    uint8_t st1 = fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();
    fdc_read_byte();

    if(st0 & 0xC0 || st1 != 0){
        kprintf(KPRINTF_ERROR, "fdc: floppy write error\n");
        fdc_motor_off(drive_num);
        return 1;
    }

    fdc_motor_off(drive_num);
    drives[drive_num].cached_cyl = -1;
    drives[drive_num].cached_head = -1;
    return 0;
}

void lba_to_chs(uint64_t lba, uint16_t* cyl, uint16_t* head, uint16_t* sector){
    *cyl    = lba / (2 * FLOPPY_SECTORS_PER_TRACK);
    *head   = ((lba % (2 * FLOPPY_SECTORS_PER_TRACK)) / FLOPPY_SECTORS_PER_TRACK);
    *sector = ((lba % (2 * FLOPPY_SECTORS_PER_TRACK)) % FLOPPY_SECTORS_PER_TRACK + 1);
}

void floppy_read_sectors(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    uint8_t* dest = buffer;
    uint16_t remaining_sectors = sector_count;

    while(remaining_sectors > 0){
        uint16_t cyl;
        uint16_t head;
        uint16_t sector;

        lba_to_chs(lba, &cyl, &head, &sector);

        floppy_read_track(device_no, cyl, head);
        uint16_t sectors_available = FLOPPY_SECTORS_PER_TRACK - (sector - 1);
        uint16_t read_count;

        if(remaining_sectors < sectors_available){
            read_count = remaining_sectors;
        }
        else{
            read_count = sectors_available;
        }
        
        memcpy(dest,drives[device_no].track_buffer + ((sector - 1) * BYTES_PER_SECTOR),read_count * BYTES_PER_SECTOR);

        dest += read_count * BYTES_PER_SECTOR;
        lba += read_count;
        remaining_sectors -= read_count;
    }
}

void floppy_write_sectors(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    uint8_t* dest = buffer;
    uint16_t remaining_sectors = sector_count;

    while(remaining_sectors > 0){
        uint16_t cyl;
        uint16_t head;
        uint16_t sector;

        lba_to_chs(lba, &cyl, &head, &sector);

        floppy_read_track(device_no, cyl, head);
        uint16_t sectors_available = FLOPPY_SECTORS_PER_TRACK - (sector - 1);
        uint16_t write_count;

        if(remaining_sectors < sectors_available){
            write_count = remaining_sectors;
        }
        else{
            write_count = sectors_available;
        }
        
        memcpy(drives[device_no].track_buffer + ((sector - 1) * BYTES_PER_SECTOR),dest,write_count * BYTES_PER_SECTOR);
        floppy_write_track(device_no,cyl,head);

        dest += write_count * BYTES_PER_SECTOR;
        lba += write_count;
        remaining_sectors -= write_count;
    }
}

void floppy_read(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    uint64_t flags = irq_save();
    irq_enable();
    floppy_read_sectors(device_no,lba,sector_count,buffer);
    irq_restore(flags);
}

void floppy_write(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    uint64_t flags = irq_save();
    irq_enable();
    floppy_write_sectors(device_no,lba,sector_count,buffer);
    irq_restore(flags);
}

void floppy_init(){
    int drive_count = detect_floppy_drives();

    if(drive_count == 0) 
        return;

    register_irq_handler(6,floppy_irq_handler);

    int reset_res = fdc_reset_controller();
    if(reset_res){
        kprintf(KPRINTF_ERROR,"fdc: error resetting floppy controller - timed out\n");
    }

    controller_version = fdc_get_version();
    kprintf(KPRINTF_DEBUG, "fdc: initialising controller that reported version byte 0x%x\n",controller_version);

    create_kernel_task(fdc_motor_timer);

    for(int i = 0; i < FLOPPY_MAX_DRIVES; i++){
        if(drives[i].type != FLOPPY_TYPE_NONE){
            drives[i].cached_cyl = -1;
            drives[i].cached_head = -1;
            drives[i].track_buffer = malloc(BYTES_PER_SECTOR * FLOPPY_SECTORS_PER_TRACK);
                
            floppy_recalibrate(i);
            struct disk fd;
            memset(&fd,0,sizeof(struct disk));
            fd.block_size = BYTES_PER_SECTOR;
            strcpy(fd.disk_name,"Floppy drive");
            fd.internal_no = i;
            fd.type = DISK_FLOPPY;
            fd.read = floppy_read;
            fd.write = floppy_write;

            register_disk(fd);
        }
    }
}