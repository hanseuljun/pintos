#include "suppl_page_table.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static struct list suppl_page_list;

void suppl_page_table_init ()
{
  list_init (&suppl_page_list);
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

  struct suppl_page *suppl_page = malloc (sizeof *suppl_page);
  list_push_back (&suppl_page_list, &suppl_page->elem);

  return true;
}

struct suppl_page *suppl_page_table_pop (void)
{
  if (list_empty (&suppl_page_list))
    return NULL;

  struct thread *t = thread_current ();
  struct suppl_page *suppl_page = list_entry (list_pop_front (&suppl_page_list), struct suppl_page, elem);
  pagedir_clear_page (t->pagedir, suppl_page->upage);

  return suppl_page;
}
