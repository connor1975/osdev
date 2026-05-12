#include <string.h>

char to_upper(char string){
    if(string > 96){
        return string-=32;
    }
    return string;
}

char to_lower(char string){
    if(string < 96 && string > 0x40){
        return string+=32;
    }
    return string;
}

char fat_filename[11];
char* to_fat_filename(char* filename){
    memset(fat_filename,' ', 11);
    for(int i = 0; i < 8; i++){
        if(*filename == '.' || *filename == 0 || *filename == '/'){
            break;
        }
        fat_filename[i] = to_upper(*filename);
        filename++;
    }
    if(filename[0] == '.'){
        fat_filename[8] = to_upper(filename[1]);
        fat_filename[9] = to_upper(filename[2]);
        fat_filename[10] = to_upper(filename[3]);
    }
    return fat_filename;
}

void to_normal_filename(char* fat_name, char* dest) {
    char* normal_name = dest;
    int i, j;

    for (i = 0, j = 0; i < 8; i++) {
        if (fat_name[i] != ' ') {
            normal_name[j++] = to_lower(fat_name[i]);
        }
    }

    if (fat_name[8] != ' ') {
        normal_name[j++] = '.';
    }

    for (i = 8; i < 11; i++) {
        if (fat_name[i] != ' ') {
            normal_name[j++] = to_lower(fat_name[i]);
        }
    }

    normal_name[j] = '\0';
}