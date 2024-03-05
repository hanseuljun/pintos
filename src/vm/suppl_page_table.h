#ifndef SUPPL_PAGE_TABLE_H
#define SUPPL_PAGE_TABLE_H

#include <stdbool.h>
#include <stddef.h>

bool suppl_page_table_set_page (void *upage, void *kpage, bool writable);
size_t suppl_page_table_get_stack_size (void);
void suppl_page_table_set_stack_size (size_t size);

#endif /* vm/suppl_page_table.h */
