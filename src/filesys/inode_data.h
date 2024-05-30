#ifndef FILESYS_INODE_DATA_H
#define FILESYS_INODE_DATA_H

#include "devices/block.h"
#include <stdbool.h>
#include "filesys/off_t.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INODE_DISK_MAX_SECTOR_COUNT 118
#define INVALID_SECTOR UINT32_MAX

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct direct_inode_disk
  {
    block_sector_t sectors[INODE_DISK_MAX_SECTOR_COUNT];  /* First data sector. */
    off_t length;                       /* File size in bytes. */
    block_sector_t indirect_sector;
    unsigned magic;                     /* Magic number. */
    uint32_t unused[7];                 /* Not used. */
  };

struct indirect_inode_disk
  {
    block_sector_t sectors[INODE_DISK_MAX_SECTOR_COUNT];  /* First data sector. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[9];                 /* Not used. */
  };

struct inode_sector_counts
  {
    size_t direct_sector_count;
    size_t indirect_sector_count;
  };

struct inode_data
  {
    struct direct_inode_disk direct_inode_disk;
    struct indirect_inode_disk indirect_inode_disk;
  };

bool inode_data_create (block_sector_t sector, off_t length);
struct inode_data *inode_data_open (block_sector_t sector);
void inode_data_release (struct inode_data *inode_data);
off_t inode_data_length (const struct inode_data *inode_data);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
struct inode_sector_counts bytes_to_sector_counts (off_t size);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
block_sector_t byte_to_sector (const struct inode_data *inode_data, off_t pos);

#endif /* filesys/inode_data.h */
