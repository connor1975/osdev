#include <stdio.h>
#include <time.h>

int main(int argc, char** argv){
    printf("%d arguments passed to our program:\n",argc);
    for(int i = 0; i < argc; i++){
        printf("%s\n",argv[i]);
    }
}