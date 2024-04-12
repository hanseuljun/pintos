#include "buffer-cache.h"
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"

uint8_t *bounce;

void buffer_cache_init (void)
{
  bounce = malloc (BLOCK_SECTOR_SIZE);
  ASSERT (bounce != NULL);
}

void buffer_cache_done (void)
{
  free (bounce);
}

uint8_t *buffer_cache_bounce (void)
{
  return bounce;
}

void buffer_cache_read (block_sector_t sector_idx)
{
  block_read (fs_device, sector_idx, bounce);
}

void buffer_cache_write (block_sector_t sector_idx)
{
  block_write (fs_device, sector_idx, bounce);
}
