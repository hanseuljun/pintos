#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include <stdint.h>
#include "devices/block.h"

void buffer_cache_init (void);
void buffer_cache_done (void);
void *buffer_cache_bounce (void);
void *buffer_cache_get_buffer (block_sector_t sector_idx);
void buffer_cache_read (block_sector_t sector_idx);
void buffer_cache_write (block_sector_t sector_idx, void *buffer);

#endif /* filesys/buffer-cache.h */
