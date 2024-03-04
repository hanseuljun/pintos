#include "frame_table.h"

void *frame_table_get_page (enum palloc_flags flags)
{
  return palloc_get_page (PAL_USER | flags);
}