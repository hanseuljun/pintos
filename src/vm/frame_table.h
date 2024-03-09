#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <stdbool.h>
#include "threads/palloc.h"

void *frame_table_install (void *upage, enum palloc_flags, bool writable);

#endif /* vm/frame_table.h */
