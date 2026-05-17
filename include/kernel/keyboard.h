#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define SHIFT_PRESSED 0x2a
#define SHIFT_RELEASED 0xaa
#define CAPS_LOCK_PRESSED 0x3a
#define CTRL_PRESSED 0x1d
#define CTRL_RELEASED 0x9d
#define ARROW_UP_PRESSED 0x48
#define ARROW_DOWN_PRESSED 0x50
#define ARROW_RIGHT_PRESSED 0x4d
#define ARROW_LEFT_PRESSED 0x4b
#define BACKSPACE_PRESSED 0x0e
#define ALT_PRESSED 0x38
#define TAB_PRESSED 0x0f
#define ESCAPE_PRESSED 0x01
#define ENTER_PRESSED 0x1c

typedef struct input_event{
    uint8_t ascii;
    uint8_t scancode;
} input_event_t;

void kbd_init();

#endif