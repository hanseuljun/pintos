#ifndef SUPPL_PAGE_TABLE_H
#define SUPPL_PAGE_TABLE_H

#include "threads/thread.h"

struct suppl_page_elem;

void suppl_page_table_init (void);
bool suppl_page_table_add_page (void *upage, void *kpage, bool swappable, bool writable);
struct suppl_page_elem *suppl_page_table_pop_swappable (void);
void suppl_page_table_exit_thread (void);
void suppl_page_table_print (void);

tid_t suppl_page_elem_get_tid (struct suppl_page_elem *);
void *suppl_page_elem_get_upage (struct suppl_page_elem *);
void *suppl_page_elem_get_kpage (struct suppl_page_elem *);
bool suppl_page_elem_get_writable (struct suppl_page_elem *);

#endif /* vm/suppl_page_table.h */
