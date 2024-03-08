#include "suppl_page_table.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static struct list page_list;

struct page
  {
    void *upage;
    void *kpage;
    struct list_elem elem;
  };

void suppl_page_table_init ()
{
  list_init (&page_list);
}

bool suppl_page_table_set_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  if (pagedir_get_page (t->pagedir, upage) != NULL)
   {
      return false;
   }
  if (!pagedir_set_page (t->pagedir, upage, kpage, writable))
   {
      return false;
   }

  printf ("page_list size: %ld\n", list_size (&page_list));
  struct page *page = malloc (sizeof *page);
  list_push_back (&page_list, &page->elem);

  return true;
}
