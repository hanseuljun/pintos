#include "mmap_table.h"
#include <list.h>
#include "threads/malloc.h"

struct mmap_elem
  {
    int id;
    int fd;
    void *addr;
    struct list_elem elem;
  };

static int next_mmap_id;
static struct list mmap_list;

void mmap_table_init (void)
{
  next_mmap_id = 1;
  list_init (&mmap_list);
}

int mmap_table_add (int fd, void *addr)
{
  struct mmap_elem *mmap_elem = malloc (sizeof *mmap_elem);
  mmap_elem->id = next_mmap_id++;
  mmap_elem->fd = fd;
  mmap_elem->addr = addr;
  list_push_back (&mmap_list, &mmap_elem->elem);
  return mmap_elem->id;
}
