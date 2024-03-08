#include "frame_table.h"
#include "devices/block.h"

void *frame_table_get_page (enum palloc_flags flags)
{
  void *page = palloc_get_page (PAL_USER | flags);
  if (page != NULL)
    return page;

  // TODO: Implement swapping and pass page-linear. See 4.1.6 Managing the Swap Table.
  return NULL;
}