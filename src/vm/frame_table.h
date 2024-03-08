#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include "threads/palloc.h"

void frame_table_install (void *upage, enum palloc_flags);

#endif /* vm/frame_table.h */
