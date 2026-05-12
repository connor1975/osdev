#include <kernel/common.h>
#include <string.h>

char* vfs_absolute_path(char* cwd, char* path) {
    if(*path == '/') {
        path++;
        cwd = "/";
    }

    char* good_path = malloc(strlen(cwd) + 1 + strlen(path) + 1);

    strcpy(good_path, cwd);
    if(good_path[strlen(cwd) - 1] != '/') {
        strcat(good_path, "/");
    }
    strcat(good_path, path);

    char** array = NULL;
    int count = 0;
    char* save_ptr = NULL;
    char* token = strtok_r(good_path, "/",&save_ptr);
    while(token != NULL) {
        if(strcmp(token,".") == 0){
            token = strtok_r(NULL, "/",&save_ptr);
            continue;    
        }else 
        if(strcmp(token, "..") == 0) {
            if(count > 0) {
                count--;
            }
        }else {
            array = (char**)realloc(array, (count + 1) * sizeof(char*));
            array[count] = token;
            count++;
        }
        token = strtok_r(NULL, "/",&save_ptr);
    }

    if(count == 0) {
        free(array);
        free(good_path);
        char* ret = malloc(2);
        ret[0] = '/';
        ret[1] = '\0';
        return ret;
    }
    
    int final_len = 0;
    for(int i = 0; i < count; i++) {
        final_len += strlen(array[i]) + 2;
    }
    char* final_str = calloc(1,final_len);
    for(int i = 0; i < count; i++) {
        strcat(final_str, "/");
        strcat(final_str, array[i]);
    }
    free(array);
    free(good_path);
    return final_str;
}