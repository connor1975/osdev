#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>
#include <fs/vfs.h>
#include <tty.h>

#define LOG_BUFFER_SIZE (64 * 1024)

const char* log_level_names[] = {
    "FATAL",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
    "TRACE"
};

static char log_buffer[LOG_BUFFER_SIZE];
static int write_index = 0;

fs_node_t log_vfs = {0};

void write_log(char* str){
    int length = strlen(str);
    for(int i = 0; i < length; i++){
        log_buffer[write_index] = str[i];
        write_index = (write_index + 1) % LOG_BUFFER_SIZE;
        log_vfs.length = write_index;
    }
}

uint64_t read_log(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer){
    if(size == 0) return 0;
    if(offset > write_index) return 0;
    if(offset + size >= write_index){
        size = write_index - offset;
    }
    memcpy(buffer,&log_buffer[offset],size);
    return size;
}

void log_vfs_init(){
    strcpy(log_vfs.name,"klog");
    log_vfs.mask = 0666;
    log_vfs.flags = FS_CHARDEVICE;
    log_vfs.read = read_log;
    dev_add_node(&log_vfs);
}

void kprintf(int level, char* fmt, ...){
    uint64_t timestamp = get_ticks_since_boot();

    char message[256];

    va_list args;
    va_start(args, fmt);
    vsprintf(message,fmt, args);
    va_end(args);

    char final_log[256];
    
    sprintf(final_log,"[%llu] %s: %s",timestamp,log_level_names[level],message);
    
    if(level <= DEBUG_LEVEL){
        write_log(final_log);
        if(PRINT_LOGS)
            if(current_tty != NULL) printf("%s",final_log);
    }
}