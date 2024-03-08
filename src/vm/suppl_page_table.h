#ifndef SUPPL_PAGE_TABLE_H
#define SUPPL_PAGE_TABLE_H

#include <stdbool.h>
#include <stddef.h>

void suppl_page_table_init (void);
bool suppl_page_table_set_page (void *upage, void *kpage, bool writable);

#endif /* vm/suppl_page_table.h */
