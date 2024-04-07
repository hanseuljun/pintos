#include "suppl_page_table.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

struct suppl_page_elem
  {
    tid_t tid;
    void *upage;
    void *kpage;
    bool writable;
    struct list_elem elem;
  };

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
      suppl_page_elem->tid = t->tid;
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

  printf ("suppl_page_table_pop_writable list_size1: %ld\n", list_size (&writable_suppl_page_list));

  struct thread *t = thread_current ();
  struct suppl_page_elem *suppl_page_elem = list_entry (list_pop_front (&writable_suppl_page_list), struct suppl_page_elem, elem);
  pagedir_clear_page (t->pagedir, suppl_page_elem->upage);

  printf ("suppl_page_table_pop_writable list_size2: %ld\n", list_size (&writable_suppl_page_list));

  return suppl_page_elem;
}

void suppl_page_table_exit_thread (void)
{
  struct thread *t = thread_current ();
  struct list_elem *e;

  e = list_begin (&writable_suppl_page_list);
  while (e != list_end (&writable_suppl_page_list))
    {
      struct suppl_page_elem *suppl_page_elem = list_entry (e, struct suppl_page_elem, elem);
      if (t->tid == suppl_page_elem->tid)
        {
          e = list_remove (e);
        }
      else
        {
          e = list_next (e);
        }
    }
}

void suppl_page_table_print (void)
{
  struct list_elem *e;

  printf ("writable_suppl_page_list (size: %ld)\n", list_size (&writable_suppl_page_list));
  for (e = list_begin (&writable_suppl_page_list); e != list_end (&writable_suppl_page_list);
       e = list_next (e))
    {
      struct suppl_page_elem *suppl_page_elem = list_entry (e, struct suppl_page_elem, elem);
      printf ("  - tid: %d, upage: %p, kpage: %p\n",
              suppl_page_elem->tid,
              suppl_page_elem->upage,
              suppl_page_elem->kpage);
    }
}

tid_t suppl_page_elem_get_tid (struct suppl_page_elem *elem)
{
  return elem->tid;
}

void *suppl_page_elem_get_upage (struct suppl_page_elem *elem)
{
  return elem->upage;
}

void *suppl_page_elem_get_kpage (struct suppl_page_elem *elem)
{
  return elem->kpage;
}

bool suppl_page_elem_get_writable (struct suppl_page_elem *elem)
{
  return elem->writable;
}
