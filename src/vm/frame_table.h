#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include <stdbool.h>

void *frame_table_install (void *upage, bool writable);
void *frame_table_reinstall (void *upage);

#endif /* vm/frame_table.h */
