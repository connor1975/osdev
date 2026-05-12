#include <stdint.h>
#include <stddef.h>
#include <bootloader/common.h>
#include <string.h>

void* segoffset_to_linear(uint32_t addr){
    uint32_t offset = addr & 0xFFFF;
    uint32_t segment = addr >> 16;
    return (void*)(uint64_t)(segment * 16 + offset);
}

uint32_t linear_to_segoff(uint32_t addr){
    uint32_t ret;
    ret = addr % 16;
    ret = (addr / 16) << 16;
    return ret;
}

extern char end;
void* malloc_ptr = &end;

void* malloc(uint64_t size){
    size = ((size + 15) / 16) * 16;
    void* temp = malloc_ptr;
    malloc_ptr+=size;
    return temp;
}

void free(uint64_t size){
    size = ((size + 15) / 16) * 16;
    malloc_ptr-=size;
}

extern uint64_t boot_pml4[];
uint64_t* pml4 = boot_pml4;

uint64_t align(uint64_t number, uint32_t alignTo)
{
    if (alignTo == 0)
        return number;

    uint64_t rem = number % alignTo;
    return (rem > 0) ? (number + alignTo - rem) : number;
}

extern void load_cr3(void* pml4);

void setup_paging(){
    if((uint64_t)malloc_ptr % 4096) malloc_ptr = (void*)align((uint64_t)malloc_ptr,4096);
    uint64_t* pdpt = (void*)(pml4[0] & 0xFFFFFFFFFFFFF000);
    uint64_t memptr = 0;
    for(int y = 0; y < 4; y++){
        if(!pdpt[y]) pdpt[y] = (uint64_t)malloc(4096) | 3;
        uint64_t* pd = (void*)(pdpt[y] & 0xFFFFFFFFFFFFF000);
        for(int x = 0; x < 512; x++){
            uint64_t* pt = malloc(4096);
            for(int i = 0; i < 512; i++){
                pt[i] = memptr | 3;
                memptr+=4096;
            }
            pd[x] = (uint64_t)pt | 3;
        }
    }
    uint64_t* new_pml4 = malloc(4096);
    uint64_t* new_pdpt = malloc(4096);
    uint64_t* new_pd = malloc(4096);
    memcpy(new_pml4,(void*)pml4,4096);
    memcpy(new_pdpt, (void*)(pml4[0] & 0xFFFFFFFFFFFFF000), 4096);
    memcpy(new_pd, (void*)(pdpt[0] & 0xFFFFFFFFFFFFF000), 4096);
    new_pml4[0] = (uint64_t)new_pdpt | 3;
    new_pdpt[0] = (uint64_t)new_pd | 3;
    new_pml4[(DIRECT_MAP_OFFSET >> 39) & 0x1FF] = pml4[0];
    load_cr3(new_pml4);
    pml4 = new_pml4;
}

void map_memory(uint64_t virt, uint64_t phys, uint32_t flags) {
    if((uint64_t)malloc_ptr % 4096) malloc_ptr = (void*)align((uint64_t)malloc_ptr,4096);

	virt &= 0x0000FFFFFFFFFFFFF;

	int pml4off = (virt >> 39) & 0x1FF;
	int pdptoff = (virt >> 30) & 0x1FF;
	int pdoff = (virt >> 21) & 0x1FF;
	int ptoff = (virt >> 12) & 0x1FF;
	if (!pml4[pml4off]) {
		uint64_t* address = malloc(4096);
        memset((void*)address,0,4096);
		pml4[pml4off] = (uint64_t)address | flags;
	}
	uint64_t* pdpt = (uint64_t*)(pml4[pml4off] & 0xFFFFFFFFFFFFF000);

	if (!pdpt[pdptoff]) {
		uint64_t* address = malloc(4096);
        memset((void*)address,0,4096);
		pdpt[pdptoff] = (uint64_t)address | flags;
	}
	uint64_t* pd = (uint64_t*)(pdpt[pdptoff] & 0xFFFFFFFFFFFFF000);

	if (!pd[pdoff]) {
		uint64_t* address = malloc(4096);
        memset((void*)address,0,4096);
		pd[pdoff] = (uint64_t)address | flags;
	}
	uint64_t* pt = (uint64_t*)(pd[pdoff] & 0xFFFFFFFFFFFFF000);

	pt[ptoff] = phys | flags;
}

extern void* get_e820_block(uint32_t* continuation_id);

int get_memory_map_entry_count(){
    uint32_t continuation_id = 0;
    int ret = 0;
    do{
        get_e820_block(&continuation_id);
        ret++;
    } while (continuation_id != 0);
    return ret;
}

void* get_memory_map(uint32_t* entry_count){
    uint32_t entries = get_memory_map_entry_count();
    void* buffer = malloc(entries * 24);
    uint32_t continuation_id = 0;
    for(int i = 0; i < entries; i++){
        memcpy(buffer + (24 * i),get_e820_block(&continuation_id),24);
    }
    *entry_count = entries;
    return buffer;
}