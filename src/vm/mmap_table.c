#include "mmap_table.h"
#include <list.h>

static struct list mmap_list;

void mmap_table_init (void)
{
  list_init (&mmap_list);
}
