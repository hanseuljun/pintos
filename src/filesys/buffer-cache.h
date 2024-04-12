#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include <stdint.h>
#include "devices/block.h"

void buffer_cache_init (void);
void buffer_cache_done (void);
uint8_t *buffer_cache_bounce (void);
uint8_t *buffer_cache_buffer (block_sector_t sector_idx);
void buffer_cache_read (block_sector_t sector_idx);
void buffer_cache_write (block_sector_t sector_idx);

#endif /* filesys/buffer-cache.h */
