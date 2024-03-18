#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include <hash.h>
#include "devices/block.h"
#include "threads/thread.h"

struct swap_table_elem;

void swap_table_init (void);
void swap_table_insert_and_save (tid_t tid, void *upage, void *kpage, bool writable);
void swap_table_load_and_remove (struct swap_table_elem *swap_table_elem, void *kpage);
/* Returns a null pointer when not found. */
struct swap_table_elem *swap_table_find (tid_t tid, void *upage);

bool swap_table_elem_is_writable (struct swap_table_elem *swap_table_elem);

#endif /* vm/swap_table.h */
