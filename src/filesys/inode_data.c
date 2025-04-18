#include "filesys/inode_data.h"
#include <math.h>
#include <round.h>
#include <string.h>
#include "filesys/fs-cache.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

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
    block_sector_t parent_doubly_indirect_sector;
    unsigned magic;                     /* Magic number. */
    uint32_t unused[6];                 /* Not used. */
  };

struct indirect_inode_disk
  {
    block_sector_t sectors[INODE_DISK_MAX_SECTOR_COUNT];  /* First data sector. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[9];                 /* Not used. */
  };

struct inode_data
  {
    struct direct_inode_disk direct_inode_disk;
    struct indirect_inode_disk indirect_inode_disk;
    struct indirect_inode_disk parent_doubly_indirect_inode_disk;
    struct indirect_inode_disk *children_doubly_indirect_inode_disk[INODE_DISK_MAX_SECTOR_COUNT];
  };

struct inode_sector_counts
  {
    size_t direct_sector_count;
    size_t indirect_sector_count;
    size_t doubly_indirect_sector_count;  /* Number of sectors in children nodes. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline struct inode_sector_counts
bytes_to_sector_counts (off_t size)
{
  size_t left_sector_count = DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);

  struct inode_sector_counts sector_counts;
  sector_counts.direct_sector_count = MIN(left_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
  left_sector_count -= sector_counts.direct_sector_count;
  sector_counts.indirect_sector_count = MIN(left_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
  left_sector_count -= sector_counts.indirect_sector_count;
  sector_counts.doubly_indirect_sector_count = left_sector_count;

  return sector_counts;
}

bool allocate_inode_data_disks (struct inode_data *inode_data, off_t length);
void write_inode_data_disks (struct inode_data *inode_data, block_sector_t direct_sector, off_t length);

bool
inode_data_create (block_sector_t sector, off_t length)
{
  ASSERT (lock_held_by_current_thread (fs_cache_get_lock ()));

  struct inode_data *inode_data = calloc (1, sizeof *inode_data);
  if (inode_data == NULL)
    return false;

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof inode_data->direct_inode_disk == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof inode_data->indirect_inode_disk == BLOCK_SECTOR_SIZE);

  inode_data->direct_inode_disk.length = length;
  inode_data->direct_inode_disk.indirect_sector = INVALID_SECTOR;
  inode_data->direct_inode_disk.magic = INODE_MAGIC;
  inode_data->indirect_inode_disk.magic = INODE_MAGIC;

  if (!allocate_inode_data_disks (inode_data, length))
    {
      free (inode_data);
      return false;
    }

  write_inode_data_disks (inode_data, sector, length);
  free (inode_data);
  return true;
}

bool allocate_inode_data_disks (struct inode_data *inode_data, off_t length)
{
  ASSERT (lock_held_by_current_thread (fs_cache_get_lock ()));

  struct inode_sector_counts sector_counts = bytes_to_sector_counts (length);
  inode_data->direct_inode_disk.length = length;
  inode_data->direct_inode_disk.indirect_sector = INVALID_SECTOR;
  inode_data->direct_inode_disk.parent_doubly_indirect_sector = INVALID_SECTOR;
  inode_data->direct_inode_disk.magic = INODE_MAGIC;

  for (size_t i = 0; i < sector_counts.direct_sector_count; i++)
    {
      if (!free_map_allocate(1, &inode_data->direct_inode_disk.sectors[i]))
        return false;
    }

  if (sector_counts.indirect_sector_count > 0)
    {
      if (!free_map_allocate(1, &inode_data->direct_inode_disk.indirect_sector))
        return false;

      inode_data->indirect_inode_disk.magic = INODE_MAGIC;

      for (size_t i = 0; i < sector_counts.indirect_sector_count; i++)
        {
          if (!free_map_allocate(1, &inode_data->indirect_inode_disk.sectors[i]))
            return false;
        }
    }

  if (sector_counts.doubly_indirect_sector_count > 0)
    {
      if (!free_map_allocate(1, &inode_data->direct_inode_disk.parent_doubly_indirect_sector))
        return false;
      
      inode_data->parent_doubly_indirect_inode_disk.magic = INODE_MAGIC;

      size_t parent_sector_index = 0;
      size_t left_children_sector_count = sector_counts.doubly_indirect_sector_count;
      while (left_children_sector_count > 0)
        {
          if (!free_map_allocate(1, &inode_data->parent_doubly_indirect_inode_disk.sectors[parent_sector_index]))
            return false;

          inode_data->children_doubly_indirect_inode_disk[parent_sector_index] = malloc (sizeof (struct indirect_inode_disk));
          inode_data->children_doubly_indirect_inode_disk[parent_sector_index]->magic = INODE_MAGIC;

          size_t child_sector_count = MIN(left_children_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
          for (size_t i = 0; i < child_sector_count; i++)
            {
              if (!free_map_allocate(1, &inode_data->children_doubly_indirect_inode_disk[parent_sector_index]->sectors[i]))
                return false;
            }
          
          parent_sector_index++;
          left_children_sector_count -= child_sector_count;
        }
    }

  return true;
}

void write_inode_data_disks (struct inode_data *inode_data, block_sector_t direct_sector, off_t length)
{
  ASSERT (lock_held_by_current_thread (fs_cache_get_lock ()));

  struct inode_sector_counts sector_counts = bytes_to_sector_counts (length);
  memcpy (fs_cache_get_buffer (direct_sector), &inode_data->direct_inode_disk, BLOCK_SECTOR_SIZE);
  fs_cache_write (direct_sector);

  for (size_t i = 0; i < sector_counts.direct_sector_count; i++)
    {
      block_sector_t sector = inode_data->direct_inode_disk.sectors[i];
      memset (fs_cache_get_buffer (sector), 0, BLOCK_SECTOR_SIZE);
      fs_cache_write (sector);
    }

  if (sector_counts.indirect_sector_count > 0)
    {
      ASSERT (inode_data->direct_inode_disk.indirect_sector != INVALID_SECTOR);

      block_sector_t indirect_sector = inode_data->direct_inode_disk.indirect_sector;
      memcpy (fs_cache_get_buffer (indirect_sector), &inode_data->indirect_inode_disk, BLOCK_SECTOR_SIZE);
      fs_cache_write (indirect_sector);
      for (size_t i = 0; i < sector_counts.indirect_sector_count; i++)
        {
          block_sector_t sector = inode_data->indirect_inode_disk.sectors[i];
          memset (fs_cache_get_buffer (sector), 0, BLOCK_SECTOR_SIZE);
          fs_cache_write (sector);
        }
    }

  if (sector_counts.doubly_indirect_sector_count > 0)
    {
      ASSERT (inode_data->direct_inode_disk.parent_doubly_indirect_sector != INVALID_SECTOR);

      block_sector_t parent_doubly_indrect_sector = inode_data->direct_inode_disk.parent_doubly_indirect_sector;
      memcpy (fs_cache_get_buffer (parent_doubly_indrect_sector), &inode_data->parent_doubly_indirect_inode_disk, BLOCK_SECTOR_SIZE);
      fs_cache_write (parent_doubly_indrect_sector);

      size_t parent_sector_index = 0;
      size_t left_children_sector_count = sector_counts.doubly_indirect_sector_count;
      while (left_children_sector_count > 0)
        {
          block_sector_t child_doubly_indrect_sector = inode_data->parent_doubly_indirect_inode_disk.sectors[parent_sector_index];
          memcpy (fs_cache_get_buffer (child_doubly_indrect_sector), &inode_data->children_doubly_indirect_inode_disk[parent_sector_index], BLOCK_SECTOR_SIZE);
          fs_cache_write (child_doubly_indrect_sector);

          size_t child_sector_count = MIN(left_children_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
          for (size_t i = 0; i < child_sector_count; i++)
            {
              block_sector_t sector = inode_data->children_doubly_indirect_inode_disk[parent_sector_index]->sectors[i];
              memset (fs_cache_get_buffer (sector), 0, BLOCK_SECTOR_SIZE);
              fs_cache_write (sector);
            }

          parent_sector_index++;
          left_children_sector_count -= child_sector_count;
        }
    }
}

struct inode_data *
inode_data_open (block_sector_t sector)
{
  struct inode_data *inode_data = malloc (sizeof *inode_data);
  if (inode_data == NULL)
    return NULL;

  lock_acquire (fs_cache_get_lock ());

  fs_cache_read (sector);
  memcpy (&inode_data->direct_inode_disk, fs_cache_get_buffer (sector), BLOCK_SECTOR_SIZE);
  ASSERT (inode_data->direct_inode_disk.magic == INODE_MAGIC);

  struct inode_sector_counts sector_counts = bytes_to_sector_counts (inode_data->direct_inode_disk.length);
  if (sector_counts.indirect_sector_count > 0)
    {
      block_sector_t indirect_sector = inode_data->direct_inode_disk.indirect_sector;
      ASSERT (indirect_sector != INVALID_SECTOR);

      fs_cache_read (indirect_sector);
      memcpy (&inode_data->indirect_inode_disk, fs_cache_get_buffer (indirect_sector), BLOCK_SECTOR_SIZE);
      ASSERT (inode_data->indirect_inode_disk.magic == INODE_MAGIC);
    }

  if (sector_counts.doubly_indirect_sector_count > 0)
    {
      block_sector_t parent_doubly_indirect_sector = inode_data->direct_inode_disk.parent_doubly_indirect_sector;
      ASSERT (parent_doubly_indirect_sector != INVALID_SECTOR);

      fs_cache_read (parent_doubly_indirect_sector);
      memcpy (&inode_data->parent_doubly_indirect_inode_disk, fs_cache_get_buffer (parent_doubly_indirect_sector), BLOCK_SECTOR_SIZE);
      ASSERT (inode_data->parent_doubly_indirect_inode_disk.magic == INODE_MAGIC);

      size_t parent_sector_index = 0;
      size_t left_children_sector_count = sector_counts.doubly_indirect_sector_count;
      while (left_children_sector_count > 0)
        {
          block_sector_t child_doubly_indrect_sector = inode_data->parent_doubly_indirect_inode_disk.sectors[parent_sector_index];
          fs_cache_read (child_doubly_indrect_sector);
          memcpy (&inode_data->children_doubly_indirect_inode_disk[parent_sector_index], fs_cache_get_buffer (child_doubly_indrect_sector), BLOCK_SECTOR_SIZE);
          ASSERT (inode_data->children_doubly_indirect_inode_disk[parent_sector_index]->magic == INODE_MAGIC);

          size_t child_sector_count = MIN(left_children_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
          parent_sector_index++;
          left_children_sector_count -= child_sector_count;
        }
    }

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

  size_t parent_sector_index = 0;
  size_t left_children_sector_count = sector_counts.doubly_indirect_sector_count;
  while (left_children_sector_count > 0)
    {
      free_map_release (inode_data->parent_doubly_indirect_inode_disk.sectors[parent_sector_index], 1);

      size_t child_sector_count = MIN(left_children_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
      for (size_t i = 0; i < child_sector_count; i++)
        free_map_release (inode_data->children_doubly_indirect_inode_disk[parent_sector_index]->sectors[i], 1);

      free (inode_data->children_doubly_indirect_inode_disk[parent_sector_index]);

      parent_sector_index++;
      left_children_sector_count -= child_sector_count;
    }
}

bool
inode_data_extend (struct inode_data *inode_data, off_t length)
{
  size_t current_length = inode_data_length (inode_data);
  size_t target_length = current_length + length;

  struct inode_sector_counts current_sector_counts = bytes_to_sector_counts (current_length);
  struct inode_sector_counts target_sector_counts = bytes_to_sector_counts (target_length);

  size_t direct_sector_count_diff = target_sector_counts.direct_sector_count - current_sector_counts.direct_sector_count;
  for (size_t i = 0; i < direct_sector_count_diff; i++)
    {
      block_sector_t sector;
      if (!free_map_allocate(1, &sector))
        return false;

      inode_data->direct_inode_disk.sectors[current_sector_counts.direct_sector_count + i] = sector;
      memset (fs_cache_get_buffer (sector), 0, BLOCK_SECTOR_SIZE);
      fs_cache_write (sector);
    }

  if (target_sector_counts.indirect_sector_count > 0 && current_sector_counts.indirect_sector_count == 0)
    {
      block_sector_t sector;
      if (!free_map_allocate(1, &sector))
        return false;

      inode_data->direct_inode_disk.indirect_sector = sector;
      inode_data->indirect_inode_disk.magic = INODE_MAGIC;
      memcpy (fs_cache_get_buffer (sector), &inode_data->indirect_inode_disk, BLOCK_SECTOR_SIZE);
      fs_cache_write (sector);
    }

  size_t indirect_sector_count_diff = target_sector_counts.indirect_sector_count - current_sector_counts.indirect_sector_count;
  for (size_t i = 0; i < indirect_sector_count_diff; i++)
    {
      block_sector_t sector;
      if (!free_map_allocate(1, &sector))
        return false;

      inode_data->indirect_inode_disk.sectors[current_sector_counts.indirect_sector_count + i] = sector;
      memset (fs_cache_get_buffer (sector), 0, BLOCK_SECTOR_SIZE);
      fs_cache_write (sector);
    }

  if (target_sector_counts.doubly_indirect_sector_count > 0 && current_sector_counts.doubly_indirect_sector_count == 0)
    {
      block_sector_t sector;
      if (!free_map_allocate(1, &sector))
        return false;

      inode_data->direct_inode_disk.parent_doubly_indirect_sector = sector;
      inode_data->parent_doubly_indirect_inode_disk.magic = INODE_MAGIC;
      memcpy (fs_cache_get_buffer (sector), &inode_data->parent_doubly_indirect_inode_disk, BLOCK_SECTOR_SIZE);
      fs_cache_write (sector);
    }
  
  for (size_t doubly_indirect_sector_index = current_sector_counts.doubly_indirect_sector_count;
              doubly_indirect_sector_index < target_sector_counts.doubly_indirect_sector_count;
              doubly_indirect_sector_index++)
    {
      size_t parent_index = doubly_indirect_sector_index / INODE_DISK_MAX_SECTOR_COUNT;
      size_t child_index = doubly_indirect_sector_index % INODE_DISK_MAX_SECTOR_COUNT;
      
      if (child_index == 0)
        {
          block_sector_t sector;
          if (!free_map_allocate(1, &sector))
            return false;

          inode_data->parent_doubly_indirect_inode_disk.sectors[parent_index] = sector;
          inode_data->children_doubly_indirect_inode_disk[parent_index] = malloc (sizeof (struct indirect_inode_disk));
          inode_data->children_doubly_indirect_inode_disk[parent_index]->magic = INODE_MAGIC;
          memcpy (fs_cache_get_buffer (sector), &inode_data->children_doubly_indirect_inode_disk[parent_index], BLOCK_SECTOR_SIZE);
          fs_cache_write (sector);
        }
      
      block_sector_t sector;
      if (!free_map_allocate(1, &sector))
        return false;

      inode_data->children_doubly_indirect_inode_disk[parent_index]->sectors[child_index] = sector;
      memcpy (fs_cache_get_buffer (sector), &inode_data->children_doubly_indirect_inode_disk[parent_index]->sectors[child_index], BLOCK_SECTOR_SIZE);
      fs_cache_write (sector);
    }

  inode_data->direct_inode_disk.length += length;
  return true;
}

void
inode_data_flush (struct inode_data *inode_data, block_sector_t sector)
{
  lock_acquire (fs_cache_get_lock ());

  struct inode_sector_counts sector_counts = bytes_to_sector_counts (inode_data->direct_inode_disk.length);

  memcpy (fs_cache_get_buffer (sector), &inode_data->direct_inode_disk, BLOCK_SECTOR_SIZE);
  fs_cache_write (sector);

  if (sector_counts.indirect_sector_count > 0)
    {
      size_t indirect_sector = inode_data->direct_inode_disk.indirect_sector;
      memcpy (fs_cache_get_buffer (indirect_sector), &inode_data->indirect_inode_disk, BLOCK_SECTOR_SIZE);
      fs_cache_write (indirect_sector);
    }

  if (sector_counts.doubly_indirect_sector_count > 0)
    {
      size_t parent_doubly_indirect_sector = inode_data->direct_inode_disk.parent_doubly_indirect_sector;
      memcpy (fs_cache_get_buffer (parent_doubly_indirect_sector), &inode_data->parent_doubly_indirect_inode_disk, BLOCK_SECTOR_SIZE);
      fs_cache_write (parent_doubly_indirect_sector);

      size_t parent_doubly_indirect_sector_count = DIV_ROUND_UP(sector_counts.doubly_indirect_sector_count, INODE_DISK_MAX_SECTOR_COUNT);
      for (size_t i = 0; i <parent_doubly_indirect_sector_count; i++)
        {
          size_t sector = inode_data->parent_doubly_indirect_inode_disk.sectors[i];
          memcpy (fs_cache_get_buffer (sector), &inode_data->children_doubly_indirect_inode_disk[i], BLOCK_SECTOR_SIZE);
          fs_cache_write (sector);
        }
    }

  lock_release (fs_cache_get_lock ());
}

off_t
inode_data_length (const struct inode_data *inode_data)
{
  return inode_data->direct_inode_disk.length;
}

block_sector_t
inode_data_sector (const struct inode_data *inode_data, off_t pos) 
{
  ASSERT (inode_data != NULL);
  if (pos < inode_data->direct_inode_disk.length)
    {
      size_t index = pos / BLOCK_SECTOR_SIZE;
      if (index < INODE_DISK_MAX_SECTOR_COUNT)
        {
          return inode_data->direct_inode_disk.sectors[index];
        }
      else if (index < INODE_DISK_MAX_SECTOR_COUNT * 2)
        {
          return inode_data->indirect_inode_disk.sectors[index - INODE_DISK_MAX_SECTOR_COUNT];
        }
      else
        {
          size_t doubly_indirect_index = index - (INODE_DISK_MAX_SECTOR_COUNT * 2);
          size_t parent_index = doubly_indirect_index / INODE_DISK_MAX_SECTOR_COUNT;
          size_t child_index = doubly_indirect_index % INODE_DISK_MAX_SECTOR_COUNT;
          return inode_data->children_doubly_indirect_inode_disk[parent_index]->sectors[child_index];
        }
    }
  else
    {
      return -1;
    }
}
