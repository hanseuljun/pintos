#include "suppl_page_table.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static size_t stack_size; 

bool suppl_page_table_set_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

size_t suppl_page_table_get_stack_size (void)
{
  return stack_size;
}

void suppl_page_table_set_stack_size (size_t size)
{
  stack_size = size;
}
