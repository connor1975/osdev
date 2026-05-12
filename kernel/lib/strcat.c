#include <string.h>

char *strcat(char *destination, const char *source){
    memcpy(destination + strlen(destination),(void*)source,strlen(source) + 1);
    return destination;
}