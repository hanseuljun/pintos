#include "buffer-cache.h"
#include "devices/block.h"
#include "threads/malloc.h"

uint8_t *buffer;

void buffer_cache_init (void)
{
  buffer = malloc (BLOCK_SECTOR_SIZE);
  ASSERT (buffer != NULL);
}

void buffer_cache_done (void)
{
  free (buffer);
}

uint8_t *buffer_cache_buffer (void)
{
  return buffer;
}