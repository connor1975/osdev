#ifndef KEYBOARD_H
#define KEYBOARD_H

void kbd_init();
char* read_keyboard_string(void* bufferptr, uint64_t buffer_size);
uint32_t keyboard_read(void* bufferptr, uint64_t buffer_size);

#endif