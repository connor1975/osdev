#include <string.h>

int cursor_x = 0;
int cursor_y = 0;

void putchar(char ch){
    if(ch == '\n'){
        if(cursor_y+1 == 80){
            memset((void*)0xb8000,0,160 * 25);
            cursor_x = 0;
            cursor_y = 0;
            return;
        }
        cursor_y++;
        cursor_x = 0;
        return;
    }
    unsigned short* ptr = (unsigned short*)0xb8000 + cursor_x + (cursor_y * 80);
    *ptr = ch | (7 << 8);
    cursor_x++;
    if(cursor_x == 79){
        cursor_x = 0;
        cursor_y++;
    }
}