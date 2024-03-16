#include "frame_table.h"
#include <stdio.h>
#include "devices/block.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/suppl_page_table.h"
#include "vm/swap_table.h"

void *frame_table_install (void *upage, bool writable)
{
  ASSERT (pg_ofs (upage) == 0);

  struct thread *t = thread_current ();

  void *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL)
    {
      // enum intr_level old_level;
      // old_level = intr_disable ();

      // Swapping non-writable pages caused error messages like
      // "Interrupt 0x03 (#BP Breakpoint Exception)" described in E.8 Tips.
      // So, only swapping writable pages, at least for now.
      struct suppl_page_elem *suppl_page_elem = suppl_page_table_pop_writable (t->suppl_page_table);
      swap_table_insert_and_save (t->swap_table,
                                  suppl_page_elem_get_upage (suppl_page_elem),
                                  suppl_page_elem_get_kpage (suppl_page_elem),
                                  writable);
      palloc_free_page (suppl_page_elem_get_kpage (suppl_page_elem));

      /* Try again as one of the pages has been swapped. */
      kpage = palloc_get_page (PAL_USER);
      // intr_set_level (old_level);
    }

  ASSERT (kpage != NULL);
  ASSERT (suppl_page_table_add_page(t->suppl_page_table, upage, kpage, writable));

  return kpage;
}

void *frame_table_reinstall (void *upage)
{
  ASSERT (pg_ofs (upage) == 0);

  struct thread *t = thread_current ();

  struct swap_table_elem *swap_table_elem = swap_table_find (t->swap_table, upage);
  ASSERT (swap_table_elem != NULL);

  void *kpage = frame_table_install (upage, swap_table_elem_is_writable (swap_table_elem));
  swap_table_load_and_remove (t->swap_table, swap_table_elem, kpage);

  return kpage;
}
