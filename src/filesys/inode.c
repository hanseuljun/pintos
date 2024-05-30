#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/fs-cache.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

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

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline struct inode_sector_counts
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

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_data data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.direct_inode_disk.length)
    {
      size_t index = pos / BLOCK_SECTOR_SIZE;
      if (index < INODE_DISK_MAX_SECTOR_COUNT)
        return inode->data.direct_inode_disk.sectors[index];
      else
        return inode->data.indirect_inode_disk.sectors[index - INODE_DISK_MAX_SECTOR_COUNT];
    }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* A temporary variable to prevent crashing while creating inodes. */
static tid_t thread_creating_inode;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  thread_creating_inode = TID_ERROR;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct direct_inode_disk *direct_disk_inode = NULL;
  struct indirect_inode_disk *indirect_disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);
  // TODO: Remove following ASSERT check when the indexed inode is
  // properly implemented.
  ASSERT (length < (BLOCK_SECTOR_SIZE * INODE_DISK_MAX_SECTOR_COUNT * 2));

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *direct_disk_inode == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof *indirect_disk_inode == BLOCK_SECTOR_SIZE);

  direct_disk_inode = calloc (1, sizeof *direct_disk_inode);
  indirect_disk_inode = calloc (1, sizeof *indirect_disk_inode);
  lock_acquire (fs_cache_get_lock ());
  thread_creating_inode = thread_tid ();
  if (direct_disk_inode != NULL)
    {
      struct inode_sector_counts sector_counts = bytes_to_sector_counts (length);
      direct_disk_inode->length = length;
      direct_disk_inode->indirect_sector = INVALID_SECTOR;
      direct_disk_inode->magic = INODE_MAGIC;

      success = true;
      for (size_t i = 0; i < sector_counts.direct_sector_count; i++)
        {
          if (!free_map_allocate(1, &direct_disk_inode->sectors[i]))
            {
              success = false;
              break;
            }
        }

      if (sector_counts.indirect_sector_count > 0)
      {
        if (success)
          {
            if (!free_map_allocate(1, &direct_disk_inode->indirect_sector))
              success = false;
          }
        if (success)
          {
            for (size_t i = 0; i < sector_counts.indirect_sector_count; i++)
              {
                if (!free_map_allocate(1, &indirect_disk_inode->sectors[i]))
                  {
                    success = false;
                    break;
                  }
              }
          }
      }

      if (success) 
        {
          memcpy (fs_cache_get_buffer (sector), direct_disk_inode, BLOCK_SECTOR_SIZE);
          fs_cache_write (sector);

          for (size_t i = 0; i < sector_counts.direct_sector_count; i++)
            {
              memset (fs_cache_get_buffer (direct_disk_inode->sectors[i]), 0, BLOCK_SECTOR_SIZE);
              fs_cache_write (direct_disk_inode->sectors[i]);
            }

          if (direct_disk_inode->indirect_sector != INVALID_SECTOR)
            {
              memcpy (fs_cache_get_buffer (direct_disk_inode->indirect_sector), indirect_disk_inode, BLOCK_SECTOR_SIZE);
              fs_cache_write (direct_disk_inode->indirect_sector);
              for (size_t i = 0; i < sector_counts.indirect_sector_count; i++)
                {
                  memset (fs_cache_get_buffer (indirect_disk_inode->sectors[i]), 0, BLOCK_SECTOR_SIZE);
                  fs_cache_write (indirect_disk_inode->sectors[i]);
                }
            }

          success = true; 
        }
      free (direct_disk_inode);
      free (indirect_disk_inode);
    }
  thread_creating_inode = TID_ERROR;
  lock_release (fs_cache_get_lock ());
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_acquire (fs_cache_get_lock ());
  fs_cache_read (inode->sector);
  memcpy (&inode->data.direct_inode_disk, fs_cache_get_buffer (inode->sector), BLOCK_SECTOR_SIZE);
  if (inode->data.direct_inode_disk.indirect_sector != INVALID_SECTOR)
    memcpy (&inode->data.indirect_inode_disk, fs_cache_get_buffer (inode->data.direct_inode_disk.indirect_sector), BLOCK_SECTOR_SIZE);
  lock_release (fs_cache_get_lock ());
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          struct inode_sector_counts sector_counts = bytes_to_sector_counts (inode->data.direct_inode_disk.length);
          free_map_release (inode->sector, 1);

          for (size_t i = 0; i < sector_counts.direct_sector_count; i++)
            free_map_release (inode->data.direct_inode_disk.sectors[i], 1);
          for (size_t i = 0; i < sector_counts.indirect_sector_count; i++)
            free_map_release (inode->data.indirect_inode_disk.sectors[i], 1);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  lock_acquire (fs_cache_get_lock ());
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      fs_cache_read (sector_idx);
      memcpy (buffer + bytes_read, fs_cache_get_buffer (sector_idx) + sector_ofs, chunk_size);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  lock_release (fs_cache_get_lock ());

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  if (thread_creating_inode != thread_tid ())
    lock_acquire (fs_cache_get_lock ());
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* If the sector contains data before or after the chunk
          we're writing, then we need to read in the sector
          first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
        fs_cache_read (sector_idx);
      else
        memset (fs_cache_get_buffer (sector_idx), 0, BLOCK_SECTOR_SIZE);
      memcpy (fs_cache_get_buffer (sector_idx) + sector_ofs, buffer + bytes_written, chunk_size);
      fs_cache_write (sector_idx);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  if (thread_creating_inode != thread_tid ())
    lock_release (fs_cache_get_lock ());

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.direct_inode_disk.length;
}
