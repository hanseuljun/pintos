#ifndef FILESYS_INODE_DATA_H
#define FILESYS_INODE_DATA_H

#include "devices/block.h"
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

struct inode_data *inode_data_open (block_sector_t sector);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
struct inode_sector_counts bytes_to_sector_counts (off_t size);

#endif /* filesys/inode_data.h */
