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

struct buffer_cache_elem *buffer_cache_find (block_sector_t sector_idx);
struct buffer_cache_elem *buffer_cache_install (block_sector_t sector_idx);

struct buffer_cache_elem *buffer_cache_elem_create (block_sector_t sector_idx);
void buffer_cache_elem_destroy (struct buffer_cache_elem *elem);

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

void *buffer_cache_bounce (void)
{
  return bounce;
}

void *buffer_cache_get_buffer (block_sector_t sector_idx)
{
  struct buffer_cache_elem *elem = buffer_cache_find (sector_idx);
  ASSERT (elem != NULL);
  return elem->buffer;
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
  struct buffer_cache_elem *elem = buffer_cache_elem_create (sector_idx);
  list_push_back (&buffer_list, &elem->list_elem);

  if (list_size (&buffer_list) > MAX_BUFFER_LIST_SIZE)
    {
      struct buffer_cache_elem *e = list_entry (list_pop_front (&buffer_list), struct buffer_cache_elem, list_elem);
      buffer_cache_elem_destroy (e);
    }

  return elem;
}

struct buffer_cache_elem *buffer_cache_elem_create (block_sector_t sector_idx)
{
  struct buffer_cache_elem *elem = malloc (sizeof (*elem));
  elem->sector_idx = sector_idx;
  elem->buffer = malloc (BLOCK_SECTOR_SIZE);
  return elem;
}

void buffer_cache_elem_destroy (struct buffer_cache_elem *elem)
{
  free (elem->buffer);
  free (elem);
}
