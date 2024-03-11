#ifndef SUPPL_PAGE_TABLE_H
#define SUPPL_PAGE_TABLE_H

#include <list.h>
#include <stdbool.h>
#include <stddef.h>

struct suppl_page_elem;

void suppl_page_table_init (void);
bool suppl_page_table_add_page (void *upage, void *kpage, bool writable);
struct suppl_page_elem *suppl_page_table_pop_writable (void);

void *suppl_page_elem_get_upage (struct suppl_page_elem *);
void *suppl_page_elem_get_kpage (struct suppl_page_elem *);

#endif /* vm/suppl_page_table.h */
