#include "buffer-cache.h"
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static uint8_t *buffer;
static struct lock buffer_lock;

void buffer_cache_init (void)
{
  buffer = malloc (BLOCK_SECTOR_SIZE);
  lock_init (&buffer_lock);
  ASSERT (buffer != NULL);
}

void buffer_cache_done (void)
{
  free (buffer);
}

uint8_t *buffer_cache_get_buffer (void)
{
  return buffer;
}

struct lock *buffer_cache_get_lock (void)
{
  return &buffer_lock;
}

void buffer_cache_read (block_sector_t sector_idx)
{
  block_read (fs_device, sector_idx, buffer);
}

void buffer_cache_write (block_sector_t sector_idx)
{
  block_write (fs_device, sector_idx, buffer);
}
