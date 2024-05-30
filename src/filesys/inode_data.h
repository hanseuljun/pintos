#ifndef FILESYS_INODE_DATA_H
#define FILESYS_INODE_DATA_H

#include "devices/block.h"
#include <stdbool.h>
#include "filesys/off_t.h"

struct inode_data;

bool inode_data_create (block_sector_t sector, off_t length);
struct inode_data *inode_data_open (block_sector_t sector);
void inode_data_release (struct inode_data *inode_data);
bool inode_data_extend (struct inode_data *inode_data, off_t length);
void inode_data_flush (struct inode_data *inode_data, block_sector_t sector);
off_t inode_data_length (const struct inode_data *inode_data);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
block_sector_t inode_data_sector (const struct inode_data *inode_data, off_t pos);

#endif /* filesys/inode_data.h */
