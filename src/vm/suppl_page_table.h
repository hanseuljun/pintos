#ifndef SUPPL_PAGE_TABLE_H
#define SUPPL_PAGE_TABLE_H

#include <list.h>
#include <stdbool.h>
#include <stddef.h>

struct suppl_page_table;
struct suppl_page_elem;

struct suppl_page_table *suppl_page_table_create (void);
bool suppl_page_table_add_page (struct suppl_page_table *suppl_page_table, void *upage, void *kpage, bool writable);
struct suppl_page_elem *suppl_page_table_pop_writable (struct suppl_page_table *suppl_page_table);

void *suppl_page_elem_get_upage (struct suppl_page_elem *);
void *suppl_page_elem_get_kpage (struct suppl_page_elem *);

#endif /* vm/suppl_page_table.h */
