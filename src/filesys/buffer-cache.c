#include "buffer-cache.h"
#include <list.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* From 5.3.4 Buffer Cache. */
#define MAX_BUFFER_LIST_SIZE 64

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
  lock_init (&buffer_lock);
  list_init (&buffer_list);
  struct buffer_cache_elem *elem = buffer_cache_elem_create ();
  list_push_back (&buffer_list, &elem->list_elem);
}

void buffer_cache_done (void)
{
  struct list_elem *e = list_pop_front (&buffer_list);
  struct buffer_cache_elem *elem = list_entry (e, struct buffer_cache_elem, list_elem);
  buffer_cache_elem_destroy (elem);
}

struct lock *buffer_cache_get_lock (void)
{
  return &buffer_lock;
}

uint8_t *buffer_cache_get_buffer (block_sector_t sector_idx)
{
  struct list_elem *e = list_front (&buffer_list);
  struct buffer_cache_elem *elem = list_entry (e, struct buffer_cache_elem, list_elem);
  return elem->buffer;
}

void buffer_cache_read (block_sector_t sector_idx)
{
  // TODO: Save in buffer_list, bounce, in a way clients should use
  // buffer_cache_buffer, instead of buffer_cache_bounce.
  struct list_elem *e = list_front (&buffer_list);
  struct buffer_cache_elem *elem = list_entry (e, struct buffer_cache_elem, list_elem);
  block_read (fs_device, sector_idx, elem->buffer);
}

void buffer_cache_write (block_sector_t sector_idx)
{
  struct list_elem *e = list_front (&buffer_list);
  struct buffer_cache_elem *elem = list_entry (e, struct buffer_cache_elem, list_elem);
  block_write (fs_device, sector_idx, elem->buffer);
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
