#include "fs-cache.h"
#include <list.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* From 5.3.4 Buffer Cache. */
#define MAX_BUFFER_LIST_SIZE 64

static struct lock buffer_lock;
static struct list buffer_list;

struct fs_cache_elem
  {
    block_sector_t sector_idx;
    uint8_t *buffer;
    struct list_elem list_elem;
    bool should_write;
  };

struct fs_cache_elem *find_fs_cache_elem (block_sector_t sector_idx);
struct fs_cache_elem *install_fs_cache_elem (block_sector_t sector_idx);

void fs_cache_init (void)
{
  lock_init (&buffer_lock);
  list_init (&buffer_list);
}

void fs_cache_done (void)
{
  struct list_elem *e;

  while (!list_empty (&buffer_list))
    {
      e = list_pop_front (&buffer_list);
      struct fs_cache_elem *elem = list_entry (e, struct fs_cache_elem, list_elem);
      if (elem->should_write)
        block_write (fs_device, elem->sector_idx, elem->buffer);

      free (elem->buffer);
      free (elem);
    }
}

struct lock *fs_cache_get_lock (void)
{
  return &buffer_lock;
}

uint8_t *fs_cache_get_buffer (block_sector_t sector_idx)
{
  struct fs_cache_elem *elem = find_fs_cache_elem (sector_idx);
  if (elem == NULL)
    elem = install_fs_cache_elem (sector_idx);

  return elem->buffer;
}

void fs_cache_read (block_sector_t sector_idx)
{
  if (find_fs_cache_elem (sector_idx) != NULL)
    return;

  struct fs_cache_elem *elem = install_fs_cache_elem (sector_idx);
  block_read (fs_device, sector_idx, elem->buffer);
}

void fs_cache_write (block_sector_t sector_idx)
{
  struct fs_cache_elem *elem = find_fs_cache_elem (sector_idx);
  if (elem == NULL)
    {
      elem = install_fs_cache_elem (sector_idx);
      block_read (fs_device, sector_idx, elem->buffer);
    }
  elem->should_write = true;
}

void fs_cache_flush (block_sector_t sector_idx)
{
  struct fs_cache_elem *elem = find_fs_cache_elem (sector_idx);
  if (elem == NULL)
    return;
  if (!elem->should_write)
    return;

  block_write (fs_device, sector_idx, elem->buffer);
  elem->should_write = false;
}

struct fs_cache_elem *find_fs_cache_elem (block_sector_t sector_idx)
{
  struct list_elem *e;

  for (e = list_begin (&buffer_list); e != list_end (&buffer_list);
       e = list_next (e))
    {
      struct fs_cache_elem *elem = list_entry (e, struct fs_cache_elem, list_elem);
      if (elem->sector_idx == sector_idx)
        return elem;
    }
  return NULL;
}

struct fs_cache_elem *install_fs_cache_elem (block_sector_t sector_idx)
{
  struct fs_cache_elem *elem;

  if (list_size (&buffer_list) < MAX_BUFFER_LIST_SIZE)
    {
      elem = malloc (sizeof (*elem));
      elem->buffer = malloc (BLOCK_SECTOR_SIZE);
      elem->sector_idx = sector_idx;
      elem->should_write = false;
    }
  else
    {
      struct list_elem *e = list_pop_front (&buffer_list);
      elem = list_entry (e, struct fs_cache_elem, list_elem);
      if (elem->should_write)
        block_write (fs_device, elem->sector_idx, elem->buffer);
      elem->sector_idx = sector_idx;
      elem->should_write = false;
    }
  list_push_back (&buffer_list, &elem->list_elem);
  return elem;
}
