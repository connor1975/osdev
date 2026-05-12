#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

// Simple but feature lacking printf implementation

const char* numchars = "0123456789ABCDEF";

void printf_string(char* string){
    while(*string){
        putchar(*string);
        string++;
    }
}

void printf_unsigned(uint64_t num, int base){
    if(num == 0){
        putchar('0');
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
        putchar(buffer[pos + i]);
    }
}

void printf_signed(int64_t num, int base){
    if(num < 0){
        putchar('-');
        printf_unsigned(-num,base);
    }else{
        printf_unsigned(num,base);
    }
}

#define PRINTF_LENGTH_STANDARD 0
#define PRINTF_LENGTH_LONG_LONG 1

void vprintf(char* fmt, va_list valist){
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
                case 's':
                printf_string(va_arg(valist,char*));
                break;
                case 'c':
                putchar(va_arg(valist,int));
                break;
                case 'p':
                putchar('0');
                putchar('x');
                printf_unsigned((uint64_t)va_arg(valist,void*),16);
                break;
                case '%':
                putchar('%');
                break;
            }
            fmt++;
        }else{
            putchar(*fmt);
            fmt++;
        }
    }
}

void printf(char* fmt, ...){
    va_list valist;
    va_start(valist,fmt);
    vprintf(fmt,valist);
    va_end(valist);
}