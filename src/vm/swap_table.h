#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include <hash.h>
#include <stdbool.h>
#include "devices/block.h"

struct swap_table_elem
  {
    void *upage;
    bool writable;
    block_sector_t sector;
    struct hash_elem hash_elem;
  };

void swap_table_init (void);
void swap_table_insert_and_save (void *upage, void *kpage, bool writable);
void swap_table_load_and_remove (struct swap_table_elem *swap_table_elem, void *kpage);
/* Returns a null pointer when not found. */
struct swap_table_elem *swap_table_find (void *upage);

#endif /* vm/swap_table.h */
