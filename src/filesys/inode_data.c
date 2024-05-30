#include "filesys/inode_data.h"
#include <round.h>
#include <string.h>
#include "filesys/fs-cache.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

struct inode_data *
inode_data_open (block_sector_t sector)
{
  struct inode_data *inode_data = malloc (sizeof *inode_data);
  if (inode_data == NULL)
    return NULL;

  lock_acquire (fs_cache_get_lock ());
  fs_cache_read (sector);
  memcpy (&inode_data->direct_inode_disk, fs_cache_get_buffer (sector), BLOCK_SECTOR_SIZE);
  if (inode_data->direct_inode_disk.indirect_sector != INVALID_SECTOR)
    memcpy (&inode_data->indirect_inode_disk, fs_cache_get_buffer (inode_data->direct_inode_disk.indirect_sector), BLOCK_SECTOR_SIZE);
  lock_release (fs_cache_get_lock ());

  return inode_data;
}

void
inode_data_release (struct inode_data *inode_data)
{
  struct inode_sector_counts sector_counts = bytes_to_sector_counts (inode_data->direct_inode_disk.length);

  for (size_t i = 0; i < sector_counts.direct_sector_count; i++)
    free_map_release (inode_data->direct_inode_disk.sectors[i], 1);
  for (size_t i = 0; i < sector_counts.indirect_sector_count; i++)
    free_map_release (inode_data->indirect_inode_disk.sectors[i], 1);
}

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

block_sector_t
byte_to_sector (const struct inode_data *inode_data, off_t pos) 
{
  if (pos < inode_data->direct_inode_disk.length)
    {
      size_t index = pos / BLOCK_SECTOR_SIZE;
      if (index < INODE_DISK_MAX_SECTOR_COUNT)
        return inode_data->direct_inode_disk.sectors[index];
      else
        return inode_data->indirect_inode_disk.sectors[index - INODE_DISK_MAX_SECTOR_COUNT];
    }
  else
    return -1;
}
