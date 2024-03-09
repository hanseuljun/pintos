#include "frame_table.h"
#include <stdio.h>
#include "devices/block.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "vm/suppl_page_table.h"
#include "vm/swap_table.h"

void *frame_table_install (void *upage, enum palloc_flags flags, bool writable)
{
  ASSERT (pg_ofs (upage) == 0);

  // printf ("frame_table_install - 1\n");
  void *kpage = palloc_get_page (PAL_USER | flags);
  if (kpage == NULL)
    {
      // printf ("frame_table_install - 2\n");
      // enum intr_level old_level;
      // old_level = intr_disable ();

      struct suppl_page *suppl_page = suppl_page_table_pop_front ();
      palloc_free_page (suppl_page->kpage);

      swap_table_insert_and_save (suppl_page->upage, suppl_page->kpage);

      kpage = palloc_get_page (PAL_USER | flags);
      // intr_set_level (old_level);
    }

  // printf ("frame_table_install - 3\n");
  ASSERT (kpage != NULL);
  ASSERT (suppl_page_table_add_page(upage, kpage, writable));
  // printf ("frame_table_install - 4\n");

  return kpage;
}

void *frame_table_reinstall (void *upage)
{
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (swap_table_contains (upage));

  // TODO:
  // 1. Get a new page.
  // 2. Copy the bytes from the swap block to the new page for address upage.
  // 3. Release that swap block from the swap table.
  // 4. Return the new page.
  // void *kpage = frame_table_install ();

  return NULL;
}