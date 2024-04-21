#ifndef FILESYS_FS_CACHE_H
#define FILESYS_FS_CACHE_H

#include <stdint.h>
#include "devices/block.h"

void fs_cache_init (void);
void fs_cache_done (void);
struct lock *fs_cache_get_lock (void);
uint8_t *fs_cache_get_buffer (block_sector_t sector_idx);
void fs_cache_read (block_sector_t sector_idx);
void fs_cache_write (block_sector_t sector_idx);

#endif /* filesys/fs-cache.h */
