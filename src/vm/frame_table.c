#include "frame_table.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "vm/suppl_page_table.h"

void *frame_table_install (void *upage, enum palloc_flags flags)
{
  void *kpage = palloc_get_page (PAL_USER | flags);
  if (kpage == NULL)
    return NULL;

  if (!suppl_page_table_add_page(pg_round_down (upage), kpage, true))
    {
      palloc_free_page (kpage);
      return NULL;
    }

  return kpage;

  // struct suppl_page *suppl_page = suppl_page_table_pop ();
  // TODO: Implement swapping and pass page-linear. See 4.1.6 Managing the Swap Table.
  // return NULL;
}