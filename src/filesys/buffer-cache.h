#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include <stdint.h>

void buffer_cache_init (void);
void buffer_cache_done (void);
uint8_t *buffer_cache_buffer (void);

#endif /* filesys/buffer-cache.h */
