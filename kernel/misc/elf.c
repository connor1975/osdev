#include <mm.h>
#include <common.h>
#include <multitasking.h>
#include <gdt.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>

int verify_elf(void* elfdata){
    Elf64_Ehdr* header = elfdata;
    if(memcmp(header->e_ident,ELFMAG,4) != 0) return 1; // make sure it is actually a valid elf
    
    if(header->e_machine != EM_X86_64) return 1;    // check it's for x86_64 architecture
    if(header->e_type != ET_EXEC) return 1;         // check if the elf is an executable
    return 0;
}

void* load_elf(void* elfdata,uint64_t cr3){
	Elf64_Ehdr* hdr = elfdata;
    Elf64_Phdr* phdr = elfdata + hdr->e_phoff;
    for(int i = 0; i < hdr->e_phnum; i++){
        if(phdr[i].p_type == 1){
			int pagecount = (phdr[i].p_memsz + 4095) / 4096;
            for (int c = 0; c < pagecount; c++) {
                if(!is_page_mapped((void*)phdr[i].p_vaddr + (4096 * c),phys_to_virt((void*)cr3))){
                    void* page_phys = allocate_frame();
                    map_page((void*)phdr[i].p_vaddr + (4096 * c), page_phys, 0x3 | PAGE_FLAG_USER,phys_to_virt((void*)cr3));
                    memset(phys_to_virt(page_phys),0,4096);
                }
			}
            copy_to_pml4((void*)phdr[i].p_vaddr,elfdata + phdr[i].p_offset,phdr[i].p_filesz,phys_to_virt((void*)cr3));
        }
    }
	return (void*)hdr->e_entry;
}

void push_to_stack(task_t* task, void* buffer, uint64_t length){
    task->context.rsp-=length;
    copy_to_pml4((void*)task->context.rsp,buffer,length,phys_to_virt((void*)task->cr3));
}

void push_args(task_t* task, int argc, char** argv, char** envp) {
    uint64_t* argptrs = malloc(argc * 8);
    for (int i = 0; i < argc; i++) {
        push_to_stack(task, (void*)argv[i], strlen(argv[i]) + 1);
        argptrs[i] = task->context.rsp;
    }
    task->context.rsp &= ~((uintptr_t)7);

    // NULL terminator for argv
    uint64_t null_ptr = 0;
    push_to_stack(task, &null_ptr, 8);

    push_to_stack(task, (void*)argptrs, argc * 8);
    free(argptrs);
    void* argvret = (void*)task->context.rsp;

    void* envpret = NULL;
    if (envp != NULL && *envp != NULL) {
        int count = 0;
        while (envp[count] != NULL) count++;
        uint64_t* envptrs = malloc(count * 8);
        for (int i = 0; i < count; i++) {
            push_to_stack(task, envp[i], strlen(envp[i]) + 1);
            envptrs[i] = task->context.rsp;
        }
        task->context.rsp &= ~((uintptr_t)7);

        // NULL terminator for envp
        push_to_stack(task, &null_ptr, 8);

        push_to_stack(task, envptrs, count * 8);
        free(envptrs);
        envpret = (void*)task->context.rsp;
    }

    task->context.rdi = argc;
    task->context.rsi = (uint64_t)argvret;
    task->context.rdx = (uint64_t)envpret;
}

int exec_elf(task_t* task, void* elf_data, char** argv, char** envp){
    if(verify_elf(elf_data)) return -ENOEXEC;
    
    uint64_t cr3 = (uint64_t)allocate_new_pd();
    task->context.rip = (uint64_t)load_elf(elf_data,cr3);
    free(elf_data);

    memset(task->fxsave_region,0,512);
    task->cr3 = cr3;
    task->context.rflags = 0x200; // Interrupt enable
    task->context.cs = USER_CODE | 3;
    task->context.ss = USER_DATA | 3;
    task->user = 1;
    task->mmap_ptr = USER_MMAP_START;
    task->brk = USER_HEAP_START;
    task->brk_start = task->brk;
    task->brk_next_page = task->brk;
    task->context.rsp = (uint64_t)USER_STACK_TOP;
    task->rsp0 = (uint64_t)phys_to_virt(allocate_frames(DEFAULT_STACK_SIZE_PAGES)) + (4096 * DEFAULT_STACK_SIZE_PAGES); // setup kernel stack
    void* stack_phys = allocate_frames(DEFAULT_STACK_SIZE_PAGES);
    map_pages(USER_STACK_TOP - (4096 * DEFAULT_STACK_SIZE_PAGES),stack_phys,PAGE_FLAG_RW | PAGE_FLAG_PRESENT | PAGE_FLAG_USER, phys_to_virt((void*)cr3),DEFAULT_STACK_SIZE_PAGES);
    
    int i = 0;
    while(argv[i] != NULL){
        i++;
    }
    push_args(task,i,argv,envp);    
    return 0;
}

// edits string in place
char** gen_argv(char* string){
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
    return ptrs;
}