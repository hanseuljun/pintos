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

      struct suppl_page *suppl_page = suppl_page_table_pop_front ();
      palloc_free_page (suppl_page->kpage);

      swap_table_insert_and_save (suppl_page->upage, suppl_page->kpage);

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
  ASSERT (swap_table_find (upage) != NULL);

  // TODO:
  // 1. Get a new page based on how upage was originally assigned to a page.
  // 2. Copy the bytes from the swap block to the new page for address upage.
  // 3. Release that swap block from the swap table.
  // 4. Return the new page.
  // void *kpage = frame_table_install ();

  return NULL;
}