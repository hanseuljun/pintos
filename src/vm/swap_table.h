#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include <hash.h>
#include <stdbool.h>
#include "devices/block.h"

struct swap_table;
struct swap_table_elem;

void swap_table_init (void);
struct swap_table *swap_table_create (void);
void swap_table_insert_and_save (struct swap_table *swap_table, void *upage, void *kpage, bool writable);
void swap_table_load_and_remove (struct swap_table *swap_table, struct swap_table_elem *swap_table_elem, void *kpage);
/* Returns a null pointer when not found. */
struct swap_table_elem *swap_table_find (struct swap_table *swap_table, void *upage);

bool swap_table_elem_is_writable (struct swap_table_elem *swap_table_elem);

#endif /* vm/swap_table.h */
