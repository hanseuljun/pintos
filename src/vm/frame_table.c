#include "frame_table.h"
#include <stdio.h>
#include "devices/block.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/suppl_page_table.h"
#include "vm/swap_table.h"

/* Prevents swapping in a page from the swap table,
   while having another thread also swapping in the page
   and invalidating the page from the swap block. */
struct lock frame_table_lock;

void *install_frame_table (void *upage, bool writable);

void frame_table_init (void)
{
  lock_init (&frame_table_lock);
}

void *frame_table_install (void *upage, bool writable)
{
  ASSERT (pg_ofs (upage) == 0);

  lock_acquire (&frame_table_lock);
  void *kpage = install_frame_table (upage, writable);
  lock_release (&frame_table_lock);

  printf ("frame_table_install, tid: %d, upage: %p, kpage: %p, writable: %d\n", thread_current ()->tid, upage, kpage, writable);

  return kpage;
}

void *frame_table_reinstall (void *upage)
{
  ASSERT (pg_ofs (upage) == 0);

  lock_acquire (&frame_table_lock);
  struct swap_table_elem *swap_table_elem = swap_table_find (thread_tid (), upage);
  ASSERT (swap_table_elem != NULL);

  void *kpage = install_frame_table (upage, swap_table_elem_is_writable (swap_table_elem));
  // swap_table_load_and_remove (swap_table_elem, kpage);
  swap_table_load_and_remove (swap_table_elem, kpage);
  lock_release (&frame_table_lock);

  printf ("frame_table_reinstall, tid: %d, upage: %p, kpage: %p\n", thread_current ()->tid, upage, kpage);

  if (upage == 0x8148000)
    {
      suppl_page_table_print ();
      swap_table_print ();
    }

  return kpage;
}

void frame_table_exit_thread (void)
{
  suppl_page_table_exit_thread ();
}

void *install_frame_table (void *upage, bool writable)
{
  ASSERT (pg_ofs (upage) == 0);

  void *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL)
    {
      // TODO: I don't think it is safe to allow interruptions during
      // swapping. Find a way to swap without interruptions or
      // prove why it is okay to swap with interruptions.
      // enum intr_level old_level;
      // old_level = intr_disable ();

      // Swapping non-writable pages caused error messages like
      // "Interrupt 0x03 (#BP Breakpoint Exception)" described in E.8 Tips.
      // So, only swapping writable pages, at least for now.
      struct suppl_page_elem *suppl_page_elem = suppl_page_table_pop_writable ();

      // TODO: Make suppl_page_table_pop_writable work with multiple processes running.

      printf ("suppl_page_elem uninstall tid: %d, upage: %p, kpage: %p\n",
              suppl_page_elem_get_tid (suppl_page_elem),
              suppl_page_elem_get_upage (suppl_page_elem),
              suppl_page_elem_get_kpage (suppl_page_elem));

      swap_table_insert_and_save (suppl_page_elem_get_tid (suppl_page_elem),
                                  suppl_page_elem_get_upage (suppl_page_elem),
                                  suppl_page_elem_get_kpage (suppl_page_elem),
                                  suppl_page_elem_get_writable(suppl_page_elem));
      palloc_free_page (suppl_page_elem_get_kpage (suppl_page_elem));

      /* Try again as one of the pages has been swapped. */
      kpage = palloc_get_page (PAL_USER);
      // intr_set_level (old_level);
    }

  ASSERT (kpage != NULL);
  ASSERT (suppl_page_table_add_page(upage, kpage, writable));

  return kpage;
}
