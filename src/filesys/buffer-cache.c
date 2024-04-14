#include "buffer-cache.h"
#include "devices/block.h"
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

uint8_t *buffer_cache_buffer (void)
{
  return buffer;
}

void buffer_cache_lock (void)
{
  lock_acquire (&buffer_lock);
}

void buffer_cache_unlock (void)
{
  lock_release (&buffer_lock);
}
