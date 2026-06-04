#ifndef PIPE_H
#define PIPE_H

#include <common.h>
#include <fs/vfs.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DEFAULT_PIPE_SIZE 4096
fs_node_t* create_pipe(uint64_t size);
uint64_t get_pipe_free(fs_node_t* node);

#endif