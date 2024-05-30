#include "fs-cache.h"
#include <list.h>
#include <string.h>
#include "devices/timer.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* From 5.3.4 Buffer Cache. */
#define MAX_BUFFER_LIST_SIZE 64

static struct lock buffer_lock;
static struct list buffer_list;

/* Set to -1 when there is no sector to read. */
#define NO_READ_AHEAD_SECTOR_IDX -1
static int read_ahead_sector_idx;

static struct thread *read_ahead_thread;

struct fs_cache_elem
  {
    block_sector_t sector_idx;
    uint8_t *buffer;
    struct list_elem list_elem;
    bool should_write;
  };

struct fs_cache_elem *find_fs_cache_elem (block_sector_t sector_idx);
struct fs_cache_elem *install_fs_cache_elem (block_sector_t sector_idx);

void periodic_flusher (void *aux UNUSED);
void ahead_reader (void *aux UNUSED);

void fs_cache_init (void)
{
  lock_init (&buffer_lock);
  list_init (&buffer_list);
  read_ahead_sector_idx = NO_READ_AHEAD_SECTOR_IDX;

  thread_create ("fs-cache-flush", PRI_DEFAULT, periodic_flusher, NULL);

  enum intr_level old_level;
  old_level = intr_disable ();
  tid_t read_ahead_tid = thread_create ("fs-cache-read", PRI_DEFAULT, ahead_reader, NULL);
  read_ahead_thread = thread_find (read_ahead_tid);
  intr_set_level (old_level);
}

void fs_cache_done (void)
{
  struct list_elem *e;

  while (!list_empty (&buffer_list))
    {
      e = list_pop_front (&buffer_list);
      struct fs_cache_elem *elem = list_entry (e, struct fs_cache_elem, list_elem);

      /* Figured that it is impossible to write back to the hard drive when
         the system is shutting down. Remove below if that becomes clear. */
      // if (elem->should_write)
        // block_write (fs_device, elem->sector_idx, elem->buffer);

      free (elem->buffer);
      free (elem);
    }

  // TODO: If needed, stop the periodic-flush and read-ahead threads.
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

  read_ahead_sector_idx = sector_idx + 1;
  read_ahead_thread->priority = PRI_DEFAULT;
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

void periodic_flusher (void *aux UNUSED)
{
  struct list_elem *e;

  while (true)
    {
      /* Run every second. */
      timer_sleep (TIMER_FREQ);

      lock_acquire (&buffer_lock);

      for (e = list_begin (&buffer_list); e != list_end (&buffer_list);
           e = list_next (e))
        {
          struct fs_cache_elem *elem = list_entry (e, struct fs_cache_elem, list_elem);
          if (!elem->should_write)
            continue;

          block_write (fs_device, elem->sector_idx, elem->buffer);
          elem->should_write = false;
        }

      lock_release (&buffer_lock);
    }
}

void ahead_reader (void *aux UNUSED)
{
  while (true)
    {
      lock_acquire (&buffer_lock);
      if (read_ahead_sector_idx >= 0)
        {
          if (find_fs_cache_elem (read_ahead_sector_idx) == NULL)
            {
              struct fs_cache_elem *elem = install_fs_cache_elem (read_ahead_sector_idx);
              block_read (fs_device, read_ahead_sector_idx, elem->buffer);
            }
          read_ahead_sector_idx = NO_READ_AHEAD_SECTOR_IDX;
        }
      lock_release (&buffer_lock);

      thread_set_priority (PRI_MIN);
      thread_yield ();
    }

}
