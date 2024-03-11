#include "suppl_page_table.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static struct list writable_suppl_page_list;

void suppl_page_table_init ()
{
  list_init (&writable_suppl_page_list);
}

bool suppl_page_table_add_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  if (pagedir_get_page (t->pagedir, upage) != NULL)
    return false;
  if (!pagedir_set_page (t->pagedir, upage, kpage, writable))
    return false;

  if (writable)
    {
      struct suppl_page_elem *suppl_page_elem = malloc (sizeof *suppl_page_elem);
      suppl_page_elem->upage = upage;
      suppl_page_elem->kpage = kpage;
      suppl_page_elem->writable = writable;
      list_push_back (&writable_suppl_page_list, &suppl_page_elem->elem);
    }

  return true;
}

struct suppl_page_elem *suppl_page_table_pop_writable (void)
{
  if (list_empty (&writable_suppl_page_list))
    return NULL;

  struct thread *t = thread_current ();
  struct suppl_page_elem *suppl_page_elem = list_entry (list_pop_front (&writable_suppl_page_list), struct suppl_page_elem, elem);
  pagedir_clear_page (t->pagedir, suppl_page_elem->upage);

  return suppl_page_elem;
}
