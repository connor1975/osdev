#include <efi.h>
#include <efilib.h>
#include <elf.h>
#include <stdint.h>
#include <string.h>

#define BOOT_UEFI 2
#define DIRECT_MAP_OFFSET 0xffff888000000000

struct framebuffer{
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    void* framebuffer;
};

struct efi_memory_map{
    EFI_MEMORY_DESCRIPTOR* MemoryMap;
    uint64_t MemoryMapSize;
    uint64_t MapKey;
    uint64_t DescriptorSize;
    uint64_t DescriptorVersion;
};

typedef struct bootinfo{
    int boot_type;

    // BIOS
    void* mmap;
    uint32_t mmap_entry_count;
    void* reserved_mem_start;   // Bootloader stores things that are still needed after the kernel is loaded here, like page tables
    void* reserved_mem_end;

    // UEFI
    struct efi_memory_map efi_memory_map;

    struct framebuffer* framebuffer;
    void* rsdp;
    void* initrd;
} bootinfo_t;

EFI_FILE* Volume = NULL;

void GetRoot(EFI_HANDLE ImageHandle){
    EFI_LOADED_IMAGE *LoadedImage = NULL;
    EFI_GUID LoadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3,ImageHandle,&LoadedImageGuid, &LoadedImage);
    if(EFI_ERROR(status)){
        Print(L"Failed to get loaded image protocol");
        for(;;);
    }

    Volume = LibOpenRoot(LoadedImage->DeviceHandle);
}

EFI_FILE* OpenFile(CHAR16* FileName){
    EFI_FILE* file;
    EFI_STATUS status = uefi_call_wrapper(Volume->Open, 5, Volume,&file,FileName,EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(status)){
        Print(L"Failed to open file");
        for(;;);
    }
    return file;
}

UINT64 FileSize(EFI_FILE_HANDLE FileHandle){
    UINT64 ret;
    EFI_FILE_INFO* FileInfo;
    FileInfo = LibFileInfo(FileHandle);
    ret = FileInfo->FileSize;
    FreePool(FileInfo);
    return ret;
}

void* ReadWholeFile(EFI_FILE* file){
    UINT64 BufferSize = FileSize(file);
    void* buffer;
    uefi_call_wrapper(BS->AllocatePool,3,EfiLoaderData,BufferSize,&buffer);

    EFI_STATUS status = uefi_call_wrapper(file->Read, 3,file,&BufferSize,buffer);
    if(EFI_ERROR(status)){
        Print(L"Failed to read file");
        for(;;);
    }
    return buffer;
}

struct efi_memory_map GetEfiMemoryMap(){
    struct efi_memory_map ret;
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINTN DescriptorVersion;

    uefi_call_wrapper(ST->BootServices->GetMemoryMap,5,&MemoryMapSize,MemoryMap,&MapKey,&DescriptorSize,&DescriptorVersion);
    MemoryMapSize+=(DescriptorSize * 2);
    uefi_call_wrapper(ST->BootServices->AllocatePool,3,EfiLoaderData,MemoryMapSize,&MemoryMap);
    EFI_STATUS status = uefi_call_wrapper(ST->BootServices->GetMemoryMap,5,&MemoryMapSize,MemoryMap,&MapKey,&DescriptorSize,&DescriptorVersion);
    if(EFI_ERROR(status)){
        Print(L"Failed to get memory map");
        for(;;);
    }
    ret.MemoryMap = MemoryMap;
    ret.MemoryMapSize = MemoryMapSize;
    ret.MapKey = MapKey;
    ret.DescriptorSize = DescriptorSize;
    ret.DescriptorVersion = DescriptorVersion;
    return ret;
}

void* allocate_frame(){
    EFI_PHYSICAL_ADDRESS address;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages,4,AllocateAnyPages,EfiLoaderData,1,&address);
    if(EFI_ERROR(status)){
        Print(L"Failed to allocate frame");
        for(;;);
    }
    return (void*)address;
}

uint64_t* pml4;

void MapPage(void* virt, void* phys,uint32_t flags) {
	uint64_t pml4off = ((uint64_t)virt >> 39) & 0x1FF;
	uint64_t pdptoff = ((uint64_t)virt >> 30) & 0x1FF;
	uint64_t pdoff = ((uint64_t)virt >> 21) & 0x1FF;
	uint64_t ptoff = ((uint64_t)virt >> 12) & 0x1FF;	
    if (pml4[pml4off] == 0) {
		uint64_t address = (uint64_t)allocate_frame();
		memset((void*)address, 0, 4096);
		pml4[pml4off] = address | flags;
	}
	uint64_t* pdpt = (uint64_t*)(void*)(pml4[pml4off] & 0xFFFFFFFFFFFFF000);

	if (pdpt[pdptoff] == 0) {
		uint64_t address = (uint64_t)allocate_frame();
		memset((void*)address, 0, 4096);
		pdpt[pdptoff] = address | flags;
	}
	uint64_t* pd = (uint64_t*)(void*)(pdpt[pdptoff] & 0xFFFFFFFFFFFFF000);

	if (pd[pdoff] == 0) {
		uint64_t address = (uint64_t)allocate_frame();
		memset((void*)address, 0, 4096);
		pd[pdoff] = address | flags;
	}
	uint64_t* pt = (uint64_t*)(void*)(pd[pdoff] & 0xFFFFFFFFFFFFF000);
	pt[ptoff] = (uint64_t)phys | flags;
}

void* load_kernel(void* elfdata){
	Elf64_Ehdr* hdr = elfdata;
    void* kernel_end;
    Elf64_Phdr* phdr = elfdata + hdr->e_phoff;
    for(int i = 0; i < hdr->e_phnum; i++){
        if(phdr[i].p_type == 1){
			int pagecount = (phdr[i].p_memsz + 4095) / 4096;
            EFI_PHYSICAL_ADDRESS address = phdr[i].p_paddr;
            uefi_call_wrapper(BS->AllocatePages,4,AllocateAddress,EfiLoaderData,pagecount,&address);
            memset((void*)address,0,phdr[i].p_memsz);
			memcpy((void*)address, elfdata + phdr[i].p_offset, phdr[i].p_filesz);
            for (int c = 0; c < ((phdr[i].p_memsz + 4095) / 4096); c++) {
				MapPage((void*)(phdr[i].p_vaddr + (4096 * c)), (void*)(address + (4096 * c)), 0x3);
			}
        }
    }
	return (void*)hdr->e_entry;
}

EFI_GRAPHICS_OUTPUT_PROTOCOL* GetGOP(){
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    UINTN SizeOfInfo, nativeMode;

    status = uefi_call_wrapper(gop->QueryMode, 4, gop, gop->Mode==NULL?0:gop->Mode->Mode, &SizeOfInfo, &info);
    if (status == EFI_NOT_STARTED)
        status = uefi_call_wrapper(gop->SetMode, 2, gop, 0);
    if(EFI_ERROR(status)) {
        Print(L"Unable to get native mode");
    } else {
        nativeMode = gop->Mode->Mode;
    }
    return gop;
}

void* GetRSDP(){
    EFI_CONFIGURATION_TABLE* configTable = ST->ConfigurationTable;
	void* rsdp = NULL;
	EFI_GUID Acpi2TableGuid = ACPI_20_TABLE_GUID;

	for (UINTN index = 0; index < ST->NumberOfTableEntries; index++) {
		if (CompareGuid(&configTable[index].VendorGuid, &Acpi2TableGuid)) {
			if (!CompareMem((CHAR8*)"RSD PTR ", (CHAR8*)configTable->VendorTable, 8)) {
				rsdp = (void*)configTable->VendorTable;
			}
		}
		configTable++;
	}
    return rsdp;
}

EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable){
    InitializeLib(ImageHandle, SystemTable);
    EFI_STATUS status;
    
    GetRoot(ImageHandle);

    pml4 = allocate_frame();
    uint64_t old_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(old_cr3));
    memcpy((void*)pml4, (void*)old_cr3, 4096);
    pml4[(DIRECT_MAP_OFFSET >> 39) & 0x1FF] = pml4[0];

    Volume = OpenFile(L"boot");
    EFI_FILE* file = OpenFile(L"kernel.elf");
    void* kernel_buffer = ReadWholeFile(file);
    int (*kernelentry)(void*) = load_kernel(kernel_buffer);
    file = OpenFile(L"initrd");
    void* initrd_buffer = ReadWholeFile(file);
    asm("mov %0, %%cr3" : : "r" (pml4));

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = GetGOP();

    struct framebuffer* modeinfo = AllocatePool(sizeof(struct framebuffer));
    struct bootinfo* bootinfo = AllocatePool(sizeof(struct bootinfo));
    
    void* rsdp = GetRSDP();

    struct efi_memory_map memory_map = GetEfiMemoryMap();

    bootinfo->boot_type = BOOT_UEFI;
    bootinfo->efi_memory_map = memory_map;
    bootinfo->rsdp = rsdp;
    bootinfo->initrd = initrd_buffer;
    bootinfo->framebuffer = modeinfo;

    modeinfo->width = gop->Mode->Info->HorizontalResolution;
    modeinfo->height = gop->Mode->Info->VerticalResolution;
    modeinfo->bpp = gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ? 32 : 24;
    modeinfo->pitch = gop->Mode->Info->PixelsPerScanLine * (modeinfo->bpp / 8);
    modeinfo->framebuffer = (void*)(gop->Mode->FrameBufferBase);

    uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, memory_map.MapKey);

    kernelentry((void*)bootinfo + DIRECT_MAP_OFFSET);

    return EFI_SUCCESS;
}