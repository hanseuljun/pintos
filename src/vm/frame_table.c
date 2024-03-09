#include "frame_table.h"
#include <stdio.h>
#include "devices/block.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "vm/suppl_page_table.h"
#include "vm/swap_table.h"

void *frame_table_install (void *upage, enum palloc_flags flags, bool writable)
{
  // printf ("frame_table_install - 1\n");
  void *kpage = palloc_get_page (PAL_USER | flags);
  if (kpage == NULL)
    {
      // printf ("frame_table_install - 2\n");
      // enum intr_level old_level;
      // old_level = intr_disable ();

      struct suppl_page *suppl_page = suppl_page_table_pop_front ();
      palloc_free_page (suppl_page->kpage);
      // TODO: Implement swapping and pass page-linear. See 4.1.6 Managing the Swap Table.
      // 1. Put the bytes in suppl_page->kpage inside swap blocks
      // 2. Map suppl_page->upage to the swap blocks for later.
      // Will need to bring back these swap blocks when suppl_page->upage gets accessed.
      // 3. Move on to the below palloc_get_page call since now there is an available page.
      swap_table_push (suppl_page->upage, suppl_page->kpage);

      kpage = palloc_get_page (PAL_USER | flags);
      // intr_set_level (old_level);
    }

  // printf ("frame_table_install - 3\n");
  ASSERT (kpage != NULL);
  ASSERT (suppl_page_table_add_page(pg_round_down (upage), kpage, writable));
  // printf ("frame_table_install - 4\n");

  return kpage;
}