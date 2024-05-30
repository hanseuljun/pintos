#include "filesys/inode_data.h"
#include <round.h>

struct inode_sector_counts
bytes_to_sector_counts (off_t size)
{
  size_t sector_count = DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
  struct inode_sector_counts sector_counts;
  if (sector_count < INODE_DISK_MAX_SECTOR_COUNT)
    {
      sector_counts.direct_sector_count = sector_count;
      sector_counts.indirect_sector_count = 0;
    }
  else
    {
      sector_counts.direct_sector_count = INODE_DISK_MAX_SECTOR_COUNT;
      sector_counts.indirect_sector_count = sector_count - INODE_DISK_MAX_SECTOR_COUNT;
    }
  return sector_counts;
}