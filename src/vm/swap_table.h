#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include <stdbool.h>

void swap_table_init (void);
bool swap_table_contains (void *upage);
void swap_table_insert_and_save (void *upage, void *kpage);

#endif /* vm/swap_table.h */
