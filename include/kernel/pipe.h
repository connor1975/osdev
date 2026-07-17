#ifndef PIPE_H
#define PIPE_H

#include <fs/vfs.h>
#include <stdint.h>

#define DEFAULT_PIPE_SIZE 4096
fs_node_t* create_pipe(uint64_t size);
uint64_t get_pipe_free(fs_node_t* node);

#endif