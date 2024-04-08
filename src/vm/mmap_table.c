#include "mmap_table.h"
#include <list.h>
#include <stdio.h>
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame_table.h"
#include "vm/swap_table.h"

struct mmap_elem
  {
    int id;
    struct file *file;
    int tid;
    void *uaddr;
    int filesize;
    struct list_elem elem;
  };

static int next_mmap_id;
static struct list mmap_list;

static struct mmap_elem *mmap_table_find (void *uaddr);
static void mmap_table_update_file (struct mmap_elem *mmap_elem);

void mmap_table_init (void)
{
  next_mmap_id = 1;
  list_init (&mmap_list);
}

int mmap_table_add (struct file *file, void *uaddr, int filesize)
{
  struct mmap_elem *mmap_elem = malloc (sizeof *mmap_elem);
  mmap_elem->id = next_mmap_id++;
  mmap_elem->file = file_reopen (file);
  mmap_elem->tid = thread_current ()->tid;
  mmap_elem->uaddr = uaddr;
  mmap_elem->filesize = filesize;
  list_push_back (&mmap_list, &mmap_elem->elem);
  return mmap_elem->id;
}

void mmap_table_remove (int mapping)
{
  struct list_elem *e;

  printf ("mmap_table_remove - 1\n");
  e = list_begin (&mmap_list);
  while (e != list_end (&mmap_list))
    {
      struct mmap_elem *mmap_elem = list_entry (e, struct mmap_elem, elem);
      if (mmap_elem->id != mapping)
        continue;

      mmap_table_update_file (mmap_elem);
      file_close (mmap_elem->file);
      e = list_remove (e);
      return;
    }
  printf ("mmap_table_remove - 2\n");

  NOT_REACHED ();
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

  for (e = list_begin (&mmap_list); e != list_end (&mmap_list);
       e = list_next (e))
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

void mmap_table_update_file (struct mmap_elem *mmap_elem)
{
  struct thread *t = thread_current ();
  uint32_t *pd = t->pagedir;
  for (int offset = 0; offset < mmap_elem->filesize; offset += PGSIZE)
    {
      // Need to reinstall before starting to writing on file.
      // Since writing requires acquiring the lock from the ata_disk's channel
      // in ide_write, we need to do the obtaining beforehands.
      // Otherwise, since obtaining also requires the lock by going through
      // ide_read, there is a dead-lock.
      // It is possible there was no access to some chunk of the file.
      // In such case, there will be no page available for uaddr and also no
      // corresponding file found from the swap table. Just move on in this case.
      void *uaddr = mmap_elem->uaddr + offset;
      if (pagedir_get_page (pd, uaddr) == NULL)
        {
          if (swap_table_find (t->tid, uaddr) != NULL)
            frame_table_reinstall (uaddr);
          else
            continue;
        }

      file_write_at (mmap_elem->file, uaddr, PGSIZE, offset);
    }
}
