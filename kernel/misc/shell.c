#include <kernel/pci.h>
#include <kernel/common.h>
#include <kernel/multitasking.h>
#include <kernel/keyboard.h>
#include <kernel/disk.h>
#include <kernel/mm.h>
#include <kernel/time.h>
#include <elf.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

char strbuffer[128];

void shell_ls(){
    struct dirent* dirent;
    int i = 0;
    while(1){
        fs_node_t* current_directory = kopen(".");
        dirent = readdir_fs(current_directory,i);
        i++;
        if(dirent == NULL) break;
        printf("%d ",dirent->ino);
        printf("%s\n",dirent->name);
    }
}

void shell_cat(int argc, char** argv){
    if(argv == NULL ) return;
    fs_node_t* file = kopen(argv[1]);
    if(file == NULL){
        printf("file not found %s\n",argv[1]);
        return;
    }
    char* buffer = malloc(file->length + 1);
    read_fs(file,0,file->length,(void*)buffer);
    buffer[file->length] = 0;
    printf("%s",buffer);
    free(buffer);
    return;
}

char** parse_args(int* argc, char* string){
    char** ptrs = malloc(sizeof(char*));
    int size = sizeof(char*);
    int i = 1;
    ptrs[0] = string;
    while(*string != 0){
        if(*string == ' '){
            size+=sizeof(char*);
            ptrs = realloc(ptrs,size);
            ptrs[i] = string+1;
            i++;
            *string = 0;
        }
        string++;
    }
    size+=sizeof(char*);
    ptrs = realloc(ptrs,size);
    ptrs[i] = 0;
    *argc = i;
    return ptrs;
}

void print_memory_info(){
    printf("%llu MB used\n%llu MB free\n",((get_used_frame_count() * 4096) / 1024) / 1024,((get_free_frame_count() * 4096) / 1024) / 1024);
}

void list_disks(){
    struct disk* disk;
    int i = 0;
    while(1){
        disk = get_disk_info(i);
        if(disk == NULL) break;

        printf("Disk %d %s\n    %s\n",i,disk_type_strings[i],disk->disk_name);
        i++;
    }
}

int do_builtin_commands(char* command, int argc, char** argv){
    if(memcmp(command,"disks",6) == 0){
        list_disks();
        return 1;
    }
    if(memcmp(command,"ls",3) == 0){
        shell_ls();
        return 1;
    }
    if(memcmp(command,"time",5) == 0){
        printf("Unix Time: %llu\n",get_unix_time());
        return 1;
    }
    if(memcmp(command,"lspci",6) == 0){
        enumerate_pci();
        return 1;
    }
    if(memcmp(command,"meminfo",7) == 0){
        print_memory_info();
        return 1;
    }
    if(memcmp(command,"clear",6) == 0){
        clear_screen(0);
        return 1;
    }
    if(memcmp(command,"cat",4) == 0){
        shell_cat(argc,argv);
        return 1;
    }
    if(memcmp(command,"help",5) == 0){
        printf("Builtin Commands:\ndisks  -    lists detected disks\ncd     -    changes current directory\ncat    -    prints the contents of a file\nls     -    prints the contents of the current directory\nclear  -    clears the screen\nlspci  -    prints pci device info\nmeminfo-    prints memory information\ntime   -    prints time\n");
        return 1;
    }
    if(memcmp(command,"cd",3) == 0){
        if(argv == NULL) return 1;
        task_chdir(argv[1]);
        return 1;
    }
    return 0;
}

void start_shell(){
    while(1){
        printf("%s$ ",current_task->cwd);
        char* str = strbuffer;
        int len = keyboard_read(str,127);
        strbuffer[len - 1] = 0;

        int argc;
        char** argv = parse_args(&argc,str);
        char* command = str;
        
        if(do_builtin_commands(command,argc,argv)){
            free(argv);
            continue;   
        }

        fs_node_t* file = kopen(str);
        if(file == NULL){
            printf("file or command not found %s\n",command);
            free(argv);
            continue;
        }

        int pid = spawn_elf(file,argv,NULL);
        if(pid < 0){
            printf("Not executable\n");
            free(argv);
            continue;
        }

        free(argv);
        while(get_task_state(pid) != TASK_DEAD);
    }
}