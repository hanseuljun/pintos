#include "frame_table.h"
#include "devices/block.h"
#include "vm/suppl_page_table.h"

void *frame_table_get_page (enum palloc_flags flags)
{
  void *page = palloc_get_page (PAL_USER | flags);
  if (page != NULL)
    return page;

  struct suppl_page *suppl_page = suppl_page_table_pop ();
  // TODO: Implement swapping and pass page-linear. See 4.1.6 Managing the Swap Table.
  return NULL;
}