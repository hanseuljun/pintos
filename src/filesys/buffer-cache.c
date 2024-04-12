#include "buffer-cache.h"
#include <string.h>
#include "filesys/filesys.h"
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

void buffer_cache_read (block_sector_t sector_idx)
{
  block_read (fs_device, sector_idx, buffer);
}

void buffer_cache_write (block_sector_t sector_idx)
{
  block_write (fs_device, sector_idx, buffer);
}
