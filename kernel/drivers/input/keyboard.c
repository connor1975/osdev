#include <kernel/common.h>
#include <kernel/interrupts.h>
#include <kernel/pipe.h>
#include <stdint.h>
#include <stdio.h>

const char scanmap[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0,   0,  0, 0, 0, 0, 0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2',
    '3', '0', '.', 0, 0, 0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0
};

const char scanmap_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0,   0,  0, 0, 0, 0, 0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2',
    '3', '0', '.', 0, 0, 0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define SHIFT_PRESSED 0x2a
#define SHIFT_RELEASED 0xaa
#define CAPS_LOCK_PRESSED 0x3a
#define CTRL_PRESSED 0x1d
#define CTRL_RELEASED 0x9d

volatile int shifting = 0;
int last_char;
volatile int key_pressed = 0;

#define BUFFER_SIZE 16

fs_node_t* keyboard_pipe = NULL;

int kbd_cur = 0;

// stdin read
uint32_t keyboard_read(void* bufferptr, uint64_t buffer_size){
    kbd_cur = 0;
    char* buffer = bufferptr;
    char c;
    int i = 0;
    while(1){
        if( i < buffer_size){
            while(get_pipe_free(keyboard_pipe) == BUFFER_SIZE);
            key_pressed = 0;
            read_fs(keyboard_pipe,0,1,(void*)&c);
            if(c == '\b'){
                if(i == 0)continue;
                i--;
                buffer[i] = 0;
                continue;
            }
            buffer[i++] = c;
            if(c == '\n') return i;
            continue;
        }
        if(last_char == '\b'){
            if(kbd_cur == buffer_size - 1){
                i = kbd_cur;
            }
            read_fs(keyboard_pipe,0,2,NULL);
        }
        if(last_char == '\n') return i;
    }
}

void keyboard_irq_handler(struct interrupt_frame* frame){
    uint8_t scancode = inb(0x60);
    switch(scancode){
        case SHIFT_PRESSED:
            shifting = !shifting;
        break;
        case SHIFT_RELEASED:
            shifting = !shifting;
        break;
        case CAPS_LOCK_PRESSED:
            shifting = !shifting;
        break;
        default: 
            if(scancode < 0x80){
                if(!shifting) last_char = scanmap[scancode];
                else last_char = scanmap_shifted[scancode];
                if(get_pipe_free(keyboard_pipe) == 0) return;
                write_fs(keyboard_pipe,0,1,(void*)&last_char);
                if(last_char == '\b'){
                    if(kbd_cur > 0){
                        kbd_cur--;
                        putchar(last_char);
                    }
                }else{
                    kbd_cur++;
                    putchar(last_char);                
                }
                key_pressed = 1;
            }
        break;
    }   
}

void kbd_init(){
    keyboard_pipe = create_pipe(BUFFER_SIZE);
    register_irq_handler(1,keyboard_irq_handler);
}