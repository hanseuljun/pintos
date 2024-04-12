#include "buffer-cache.h"
#include <list.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"

/* From 5.3.4 Buffer Cache. */
#define MAX_BUFFER_LIST_SIZE 64

static uint8_t *bounce;
static struct list buffer_list;

struct buffer_cache_elem
  {
    block_sector_t sector_idx;
    uint8_t *buffer;
    struct list_elem list_elem;
  };

static struct buffer_cache_elem *buffer_cache_find (block_sector_t sector_idx);
static struct buffer_cache_elem *buffer_cache_install (block_sector_t sector_idx);

static struct buffer_cache_elem *create_buffer_cache_elem (block_sector_t sector_idx);
static void destroy_buffer_cache_elem (struct buffer_cache_elem *elem);

void buffer_cache_init (void)
{
  bounce = malloc (BLOCK_SECTOR_SIZE);
  ASSERT (bounce != NULL);
  list_init (&buffer_list);
}

void buffer_cache_done (void)
{
  free (bounce);
}

void buffer_cache_read (block_sector_t sector_idx, void *buffer)
{
  buffer_cache_read_advanced (sector_idx, 0, buffer, BLOCK_SECTOR_SIZE);
}

void buffer_cache_read_advanced (block_sector_t sector_idx, int sector_ofs, void *buffer, int size)
{
  struct buffer_cache_elem *elem = buffer_cache_find (sector_idx);
  if (elem == NULL)
    elem = buffer_cache_install (sector_idx);

  block_read (fs_device, sector_idx, elem->buffer);

  memcpy (buffer, elem->buffer + sector_ofs, size);
}

void buffer_cache_write (block_sector_t sector_idx, const void *buffer)
{
  block_write (fs_device, sector_idx, buffer);
}

void buffer_cache_write_advanced (block_sector_t sector_idx, int sector_ofs, const void *buffer, int size)
{
  struct buffer_cache_elem *elem = buffer_cache_find (sector_idx);

  /* If the sector contains data before or after the chunk
      we're writing, then we need to read in the sector
      first.  Otherwise we start with a sector of all zeros. */
  int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
  if (sector_ofs > 0 || size < sector_left)
    {
      buffer_cache_read (sector_idx, bounce);
      elem = buffer_cache_find (sector_idx);
      ASSERT (elem != NULL);
    }
  else
    {
      if (elem == NULL)
        elem = buffer_cache_install (sector_idx);
      memset (elem->buffer, 0, BLOCK_SECTOR_SIZE);
    }
  memcpy (elem->buffer + sector_ofs, buffer, size);
  buffer_cache_write (sector_idx, elem->buffer);
}

struct buffer_cache_elem *buffer_cache_find (block_sector_t sector_idx)
{
  struct list_elem *e;

  for (e = list_begin (&buffer_list); e != list_end (&buffer_list);
       e = list_next (e))
    {
      struct buffer_cache_elem *elem = list_entry (e, struct buffer_cache_elem, list_elem);
      if (elem->sector_idx == sector_idx)
        return elem;
    }
  return NULL;
}

struct buffer_cache_elem *buffer_cache_install (block_sector_t sector_idx)
{
  struct buffer_cache_elem *elem = create_buffer_cache_elem (sector_idx);
  list_push_back (&buffer_list, &elem->list_elem);

  if (list_size (&buffer_list) > MAX_BUFFER_LIST_SIZE)
    {
      struct buffer_cache_elem *e = list_entry (list_pop_front (&buffer_list), struct buffer_cache_elem, list_elem);
      destroy_buffer_cache_elem (e);
    }

  return elem;
}

struct buffer_cache_elem *create_buffer_cache_elem (block_sector_t sector_idx)
{
  struct buffer_cache_elem *elem = malloc (sizeof (*elem));
  elem->sector_idx = sector_idx;
  elem->buffer = malloc (BLOCK_SECTOR_SIZE);
  return elem;
}

void destroy_buffer_cache_elem (struct buffer_cache_elem *elem)
{
  free (elem->buffer);
  free (elem);
}
