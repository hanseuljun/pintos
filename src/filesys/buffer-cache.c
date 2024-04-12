#include "buffer-cache.h"
#include <list.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static uint8_t *bounce;
static struct lock buffer_lock;
static struct list buffer_list;

struct buffer_cache_elem
  {
    uint8_t *buffer;
    struct list_elem list_elem;
  };

struct buffer_cache_elem *buffer_cache_elem_create (void);
void buffer_cache_elem_destroy (struct buffer_cache_elem *elem);

void buffer_cache_init (void)
{
  bounce = malloc (BLOCK_SECTOR_SIZE);
  ASSERT (bounce != NULL);
  lock_init (&buffer_lock);
  list_init (&buffer_list);
}

void buffer_cache_done (void)
{
  free (bounce);
}

uint8_t *buffer_cache_get_bounce (void)
{
  return bounce;
}

struct lock *buffer_cache_get_lock (void)
{
  return &buffer_lock;
}

uint8_t *buffer_cache_buffer (block_sector_t sector_idx)
{
}

void buffer_cache_read (block_sector_t sector_idx)
{
  block_read (fs_device, sector_idx, bounce);
}

void buffer_cache_write (block_sector_t sector_idx)
{
  block_write (fs_device, sector_idx, bounce);
}

struct buffer_cache_elem *buffer_cache_elem_create (void)
{
  struct buffer_cache_elem *elem = malloc (sizeof (*elem));
  elem->buffer = malloc (BLOCK_SECTOR_SIZE);
  return elem;
}

void buffer_cache_elem_destroy (struct buffer_cache_elem *elem)
{
  free (elem->buffer);
  free (elem);
}
