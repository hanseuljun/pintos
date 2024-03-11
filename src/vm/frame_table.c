#include "frame_table.h"
#include <stdio.h>
#include "devices/block.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/suppl_page_table.h"
#include "vm/swap_table.h"

void *frame_table_install (void *upage, bool writable)
{
  ASSERT (pg_ofs (upage) == 0);

  void *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL)
    {
      // enum intr_level old_level;
      // old_level = intr_disable ();

      // Swapping non-writable pages caused error messages like
      // "Interrupt 0x03 (#BP Breakpoint Exception)" described in E.8 Tips.
      // So, only swapping writable pages, at least for now.
      struct suppl_page *suppl_page = suppl_page_table_pop_writable ();
      swap_table_insert_and_save (suppl_page->upage, suppl_page->kpage, writable);
      palloc_free_page (suppl_page->kpage);

      /* Try again as one of the pages has been swapped. */
      kpage = palloc_get_page (PAL_USER);
      // intr_set_level (old_level);
    }

  ASSERT (kpage != NULL);
  ASSERT (suppl_page_table_add_page(upage, kpage, writable));

  return kpage;
}

void *frame_table_reinstall (void *upage)
{
  ASSERT (pg_ofs (upage) == 0);

  struct swap_table_elem *swap_table_elem = swap_table_find (upage);
  ASSERT (swap_table_elem != NULL);

  void *kpage = frame_table_install (upage, swap_table_elem_is_writable (swap_table_elem));
  swap_table_load_and_remove (swap_table_elem, kpage);

  return kpage;
}
