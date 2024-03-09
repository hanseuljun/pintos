#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include <hash.h>
#include <stdbool.h>
#include "devices/block.h"

struct swap_table_elem
  {
    void *upage;
    block_sector_t sector;
    struct hash_elem hash_elem;
  };

void swap_table_init (void);
bool swap_table_contains (void *upage);
void swap_table_insert_and_save (void *upage, void *kpage);

#endif /* vm/swap_table.h */
