#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

struct dirent
{
    char name[128];
    uint32_t ino;
};

struct fs_node;

typedef uint32_t (*read_type_t)(struct fs_node*,uint32_t,uint32_t,uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*,uint32_t,uint32_t,uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef void (*create_file_type_t)(struct fs_node*, char* name);
typedef int (*ioctl_type_t)(struct fs_node *, unsigned long request, void* argp);
typedef struct dirent * (*readdir_type_t)(struct fs_node*,uint32_t);
typedef struct fs_node * (*finddir_type_t)(struct fs_node*,char *name);
typedef void (*truncate_type_t)(struct fs_node*,int length);

typedef struct fs_node
{
    char name[128];     // The filename.
    uint32_t mask;        // The permissions mask.
    uint32_t uid;         // The owning user.
    uint32_t gid;         // The owning group.
    uint64_t flags;       // Includes the node type. See #defines above.
    uint32_t inode;       // This is device-specific - provides a way for a filesystem to identify files.
    uint64_t length;      // Size of the file, in bytes.
    uint64_t impl;        // An implementation-defined number.
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
    create_file_type_t create_file;
    truncate_type_t truncate;
    ioctl_type_t ioctl;
    struct fs_node *ptr; // Used by mountpoints and symlinks.
} fs_node_t;

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x04
#define FS_BLOCKDEVICE 0x08
#define FS_PIPE        0x10
#define FS_SYMLINK     0x20
#define FS_MOUNTPOINT  0x40

extern fs_node_t *fs_root; // The root of the filesystem.

// Standard read/write/open/close functions. Note that these are all suffixed with
// _fs to distinguish them from the read/write/open/close which deal with file descriptors
// not file nodes.
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void open_fs(fs_node_t *node, uint8_t read, uint8_t write);
void close_fs(fs_node_t *node);
void truncate_fs(fs_node_t* node, int length);
struct dirent *readdir_fs(fs_node_t *node, uint32_t index);
fs_node_t *finddir_fs(fs_node_t *node, char *name);
void create_file_fs(fs_node_t* node, char* name);
int ioctl_fs(fs_node_t *node, unsigned long request, void * argp);
fs_node_t* find_file(char* path);
fs_node_t* find_file_dir(char* path);
char* get_filename_from_path(char* path);

void devfs_init();
void dev_add_node(fs_node_t* node);

void zero_init();

// use newlibs default values until we add our own fnctl.h to newlib

#define	_FREAD		0x0001	/* read enabled */
#define	_FWRITE		0x0002	/* write enabled */
#define	_FAPPEND	0x0008	/* append (writes guaranteed at the end) */
#define	_FCREAT		0x0200	/* open with file create */
#define	_FTRUNC		0x0400	/* open with truncation */

#define	O_RDONLY	0
#define	O_WRONLY	1
#define	O_RDWR		2
#define	O_APPEND	_FAPPEND
#define	O_CREAT		_FCREAT
#define	O_TRUNC		_FTRUNC

struct file_descriptor{
    fs_node_t* file;
    uint64_t offset;
    uint32_t flags;
    int refcount;
};

fs_node_t* kopen(char* path);
int vfs_mount(char* path,fs_node_t* local_root);
char* vfs_absolute_path(char* cwd, char* path);

#define S_IFMT	 0170000	//bit mask for the file type bit fields
#define S_IFSOCK 0140000	//socket
#define S_IFLNK	 0120000	//symbolic link
#define S_IFREG	 0100000	//regular file
#define S_IFBLK	 0060000	//block device
#define S_IFDIR	 0040000	//directory
#define S_IFCHR	 0020000	//character device
#define S_IFIFO	 0010000	//FIFO
#define S_ISUID	 0004000	//set UID bit
#define S_ISGID	 0002000	//set-group-ID bit (see below)
#define S_ISVTX	 0001000	//sticky bit (see below)
#define S_IRWXU	 00700	//mask for file owner permissions
#define S_IRUSR	 00400	//owner has read permission
#define S_IWUSR	 00200	//owner has write permission
#define S_IXUSR	 00100	//owner has execute permission
#define S_IRWXG	 00070	//mask for group permissions
#define S_IRGRP	 00040	//group has read permission
#define S_IWGRP	 00020	//group has write permission
#define S_IXGRP	 00010	//group has execute permission
#define S_IRWXO	 00007	//mask for permissions for others (not in group)
#define S_IROTH	 00004	//others have read permission
#define S_IWOTH	 00002	//others have write permission
#define S_IXOTH	 00001	//others have execute permission

#endif