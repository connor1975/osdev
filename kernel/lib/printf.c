#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static char* target_buffer = NULL;
static char buffer[4096];
static int buffer_ptr = 0;
static int len = 0;

const char* numchars = "0123456789ABCDEF";

static int mode = 0; // print

static void putchar_buffer(char c){
    if(c == 0) len = buffer_ptr;
    buffer[buffer_ptr] = (char)c;
    buffer_ptr++;
}

void printf_string(char* string){
    while(*string){
        putchar_buffer(*string);
        string++;
    }
}

void printf_unsigned(uint64_t num, int base){
    if(num == 0){
        putchar_buffer('0');
        return;
    }
    char buffer[64];
    int pos = 64;
    int len = 0;
    while(num != 0){
        pos--;
        uint64_t rem = num % base;
        buffer[pos] = numchars[rem];
        num /= base;
        len++;
    }
    for(int i = 0; i < len; i++){
        putchar_buffer(buffer[pos + i]);
    }
}

void printf_signed(int64_t num, int base){
    if(num < 0){
        putchar_buffer('-');
        printf_unsigned(-num,base);
    }else{
        printf_unsigned(num,base);
    }
}

#define PRINTF_LENGTH_STANDARD 0
#define PRINTF_LENGTH_LONG_LONG 1

void vprintf(char* fmt, va_list valist){
    buffer_ptr = 0;
    len = 0;
    int length;
    while(*fmt){
        if(*fmt == '%'){
            fmt++;
            // get length
            switch(*fmt){
                case 'l':   // length = ll
                if(*(fmt + 1) == 'l'){
                    length = PRINTF_LENGTH_LONG_LONG;
                    fmt+=2;
                }
                break;
                default:
                length = PRINTF_LENGTH_STANDARD;
                break;
            }

            switch(*fmt){
                case 'd':
                switch(length){
                    case PRINTF_LENGTH_STANDARD:
                    printf_signed(va_arg(valist,int),10);
                    break;
                    case PRINTF_LENGTH_LONG_LONG:
                    printf_signed(va_arg(valist,int64_t),10);
                    break;
                }
                break;
                case 'u':
                switch(length){
                    case PRINTF_LENGTH_STANDARD:
                    printf_unsigned(va_arg(valist,uint32_t),10);
                    break;
                    case PRINTF_LENGTH_LONG_LONG:
                    printf_unsigned(va_arg(valist,uint64_t),10);
                    break;
                }
                break;
                case 'x':
                switch(length){
                    case PRINTF_LENGTH_STANDARD:
                    printf_unsigned(va_arg(valist,uint32_t),16);
                    break;
                    case PRINTF_LENGTH_LONG_LONG:
                    printf_unsigned(va_arg(valist,uint64_t),16);
                    break;
                }
                break;
                case 'b':
                switch(length){
                    case PRINTF_LENGTH_STANDARD:
                    printf_unsigned(va_arg(valist,uint32_t),2);
                    break;
                    case PRINTF_LENGTH_LONG_LONG:
                    printf_unsigned(va_arg(valist,uint64_t),2);
                    break;
                }
                break;
                case 'o':
                switch(length){
                    case PRINTF_LENGTH_STANDARD:
                    printf_unsigned(va_arg(valist,uint32_t),8);
                    break;
                    case PRINTF_LENGTH_LONG_LONG:
                    printf_unsigned(va_arg(valist,uint64_t),8);
                    break;
                }
                break;
                case 's':
                printf_string(va_arg(valist,char*));
                break;
                case 'c':
                putchar_buffer(va_arg(valist,int));
                break;
                case 'p':
                putchar_buffer('0');
                putchar_buffer('x');
                printf_unsigned((uint64_t)va_arg(valist,void*),16);
                break;
                case '%':
                putchar_buffer('%');
                break;
            }
            fmt++;
        }else{
            putchar_buffer(*fmt);
            fmt++;
        }
    }
    putchar_buffer(0);
    if(mode == 0){
        char* ptr = buffer;
        while(*ptr != 0){
            putchar(*ptr);
            ptr++;
        }
    } else if(mode == 1){
        strcpy(target_buffer,(const char*)buffer);
    }
}

void printf(char* fmt, ...){
    va_list valist;
    va_start(valist,fmt);
    mode = 0;
    vprintf(fmt,valist);
    va_end(valist);
}

int sprintf(char *str, char *fmt, ...){
    va_list valist;
    va_start(valist,fmt);
    mode = 1;
    target_buffer = str;
    vprintf(fmt,valist);
    va_end(valist);
    mode = 0;
    return len;
}