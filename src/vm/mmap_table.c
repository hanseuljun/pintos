#include "mmap_table.h"
#include <list.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct mmap_elem
  {
    int id;
    struct file *file;
    int tid;
    void *uaddr;
    size_t filesize;
    struct list_elem elem;
  };

static int next_mmap_id;
static struct list mmap_list;

static struct mmap_elem *mmap_table_find (void *uaddr);

void mmap_table_init (void)
{
  next_mmap_id = 1;
  list_init (&mmap_list);
}

int mmap_table_add (struct file *file, void *uaddr, int filesize)
{
  struct mmap_elem *mmap_elem = malloc (sizeof *mmap_elem);
  mmap_elem->id = next_mmap_id++;
  mmap_elem->file = file;
  mmap_elem->tid = thread_current ()->tid;
  mmap_elem->uaddr = uaddr;
  mmap_elem->filesize = filesize;
  list_push_back (&mmap_list, &mmap_elem->elem);
  return mmap_elem->id;
}

bool mmap_table_contains (void *uaddr)
{
  return mmap_table_find (uaddr) != NULL;
}

void mmap_table_fill (void *uaddr)
{
  struct mmap_elem *mmap_elem = mmap_table_find (uaddr);
  ASSERT (mmap_elem != NULL);

  uint8_t *upage = pg_round_down (uaddr);
  file_read_at (mmap_elem->file, upage, PGSIZE, upage - (uint8_t *) mmap_elem->uaddr);
}

static struct mmap_elem *mmap_table_find (void *uaddr)
{
  struct thread *t = thread_current ();
  struct list_elem *e;

  e = list_begin (&mmap_list);
  while (e != list_end (&mmap_list))
    {
      struct mmap_elem *mmap_elem = list_entry (e, struct mmap_elem, elem);
      if (t->tid != mmap_elem->tid)
        continue;
      if (uaddr < mmap_elem->uaddr)
        continue;
      if (uaddr >= (mmap_elem->uaddr + mmap_elem->filesize))
        continue;
      return mmap_elem;
    }
  return NULL;
}