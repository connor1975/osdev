#include <io.h>
#include <keyboard.h>
#include <tty.h>
#include <interrupts.h>
#include <pipe.h>
#include <tty.h>
#include <stdint.h>
#include <stdio.h>
#include <debug.h>

#define PS2_TIMEOUT 100000
#define PS2_DATA 0x60
#define PS2_STATUS 0x64

#define ACK 0xFA
#define RESEND 0xFE
#define DATA_OUT_READY 0x1
#define SET_TYPEMATIC_RATE 0xF3

const char scanmap[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0x7f, '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0,   0,  0, 0, 0, 0, 0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2',
    '3', '0', '.', 0, 0, 0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0
};

const char scanmap_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0x7f, '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0,   0,  0, 0, 0, 0, 0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2',
    '3', '0', '.', 0, 0, 0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0
};

volatile int shifting = 0;
volatile int ctrl_pressed = 0;

#define BUFFER_SIZE 16

int kbd_cur = 0;
static int multibyte = 0;

void keyboard_irq_handler(struct interrupt_frame* frame){
    irq_disable();
    uint8_t scancode = inb(0x60);
    
    input_event_t input_event;
    input_event.ascii = 0;
    input_event.scancode = scancode;

    if(scancode == 0xE0){
        multibyte = 1;
        return;
    }
    if(multibyte){
        multibyte = 0;
        if(scancode < 0x90) // Key pressed not released
            tty_handle_input(input_event);
        return;
    }
    switch (scancode){
        case SHIFT_PRESSED:
        shifting = 1;
        break;
        case SHIFT_RELEASED:
        shifting = 0;
        break;
        case CTRL_PRESSED:
        ctrl_pressed = 1;
        break;
        case CTRL_RELEASED:
        ctrl_pressed = 0;
        break;

        default:
            if(scancode < 0x60){
                char c;
                if(shifting){
                    c = scanmap_shifted[scancode];
                }else{
                    c = scanmap[scancode];
                }
                input_event.ascii = c;

                if(ctrl_pressed) {
                    input_event.ctrl_pressed = 1;
                }else{
                    input_event.ctrl_pressed = 0;
                }
                
                tty_handle_input(input_event);
            }
            break;
    }
}

int keyboard_wait_data(){
    for(int i = 0; i < PS2_TIMEOUT; i++){
        if(inb(PS2_STATUS) & DATA_OUT_READY){
            return inb(PS2_DATA);
        }
    }
    return -1;
}

void keyboard_send_byte(uint8_t byte){
    outb(PS2_DATA,byte);
    uint8_t result = keyboard_wait_data();
    if(result != ACK)
        kprintf(KPRINTF_ERROR,"ps2: sending byte %x failed\n",byte);        
}

void kbd_init(){
    keyboard_send_byte(SET_TYPEMATIC_RATE);
    keyboard_send_byte(0x20);
    register_irq_handler(1,keyboard_irq_handler);
}