#ifndef MMAP_TABLE_H
#define MMAP_TABLE_H

#include <stdbool.h>

struct file;

void mmap_table_init (void);
int mmap_table_add (struct file *file, void *uaddr, int filesize);
bool mmap_table_contains (void *uaddr);
void mmap_table_fill (void *uaddr);

#endif /* vm/mmap_table.h */
