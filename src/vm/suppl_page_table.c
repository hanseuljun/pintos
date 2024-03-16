#include "suppl_page_table.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

struct suppl_page_table
  {
    struct list writable_suppl_page_list;
  };

struct suppl_page_elem
  {
    void *upage;
    void *kpage;
    bool writable;
    struct list_elem elem;
  };

struct suppl_page_table *suppl_page_table_create ()
{
  struct suppl_page_table *spt = malloc (sizeof *spt);
  list_init (&spt->writable_suppl_page_list);
  return spt;
}

bool suppl_page_table_add_page (struct suppl_page_table *suppl_page_table, void *upage, void *kpage, bool writable)
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
      list_push_back (&suppl_page_table->writable_suppl_page_list, &suppl_page_elem->elem);
    }

  return true;
}

struct suppl_page_elem *suppl_page_table_pop_writable (struct suppl_page_table *suppl_page_table)
{
  if (list_empty (&suppl_page_table->writable_suppl_page_list))
    return NULL;

  struct thread *t = thread_current ();
  struct suppl_page_elem *suppl_page_elem = list_entry (list_pop_front (&suppl_page_table->writable_suppl_page_list),
                                                        struct suppl_page_elem,
                                                        elem);
  pagedir_clear_page (t->pagedir, suppl_page_elem->upage);

  return suppl_page_elem;
}

void *suppl_page_elem_get_upage (struct suppl_page_elem *elem)
{
  return elem->upage;
}

void *suppl_page_elem_get_kpage (struct suppl_page_elem *elem)
{
  return elem->kpage;
}
