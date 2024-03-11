#ifndef SUPPL_PAGE_TABLE_H
#define SUPPL_PAGE_TABLE_H

#include <list.h>
#include <stdbool.h>
#include <stddef.h>

struct suppl_page_elem
  {
    void *upage;
    void *kpage;
    bool writable;
    struct list_elem elem;
  };

void suppl_page_table_init (void);
bool suppl_page_table_add_page (void *upage, void *kpage, bool writable);
struct suppl_page_elem *suppl_page_table_pop_writable (void);

#endif /* vm/suppl_page_table.h */
