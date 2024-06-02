#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <math.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/fs-cache.h"
#include "filesys/free-map.h"
#include "filesys/inode_data.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_data *data;             /* Inode content. */
  };

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* A temporary variable to prevent crashing while creating inodes. */
static tid_t inode_creating_thread;
static tid_t inode_data_extending_thread;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  inode_creating_thread = TID_ERROR;
  inode_data_extending_thread = TID_ERROR;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  bool success;

  ASSERT (length >= 0);

  lock_acquire (fs_cache_get_lock ());
  inode_creating_thread = thread_tid ();
  success = inode_data_create (sector, length);
  inode_creating_thread = TID_ERROR;
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
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  inode->data = inode_data_open (sector);
  if (inode->data == NULL)
    {
      free(inode);
      return NULL;
    }

  list_push_front (&open_inodes, &inode->elem);
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
          free_map_release (inode->sector, 1);
          inode_data_release (inode->data);
        }
      else
        {
          inode_data_flush (inode->data, inode->sector);
        }

      free (inode->data);
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
  ASSERT (inode != NULL);

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  lock_acquire (fs_cache_get_lock ());
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = inode_data_sector (inode->data, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = MIN(inode_left, sector_left);

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = MIN(size, min_left);
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
  ASSERT (inode != NULL);

  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  bool rightfully_already_locked = inode_creating_thread == thread_tid () || inode_data_extending_thread == thread_tid ();
  if (!rightfully_already_locked)
    lock_acquire (fs_cache_get_lock ());

  if (inode_length (inode) < (offset + size))
    {
      inode_data_extending_thread = thread_tid ();
      inode_data_extend (inode->data, offset + size - inode_length (inode));
      inode_data_extending_thread = TID_ERROR;
    }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = inode_data_sector (inode->data, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;

      int min_left = MIN(inode_left, sector_left);

      /* Number of bytes to actually write into this sector. */
      int chunk_size = MIN(size, min_left);
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
  if (!rightfully_already_locked)
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
  return inode_data_length (inode->data);
}

bool
inode_is_dir (struct inode *inode)
{
  unsigned magic;
  inode_read_at (inode, &magic, sizeof (magic), 0);
  return magic == DIR_MAGIC;
}
