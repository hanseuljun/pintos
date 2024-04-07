#ifndef MMAP_TABLE_H
#define MMAP_TABLE_H

void mmap_table_init (void);
int mmap_table_add (int fd, void *addr);

#endif /* vm/mmap_table.h */
