#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/fs-cache.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  fs_cache_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  fs_cache_done ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create_file (struct dir *dir, const char *name, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  return success;
}

bool
filesys_create_dir (struct dir *dir, const char *name)
{
  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 0)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open_file (struct dir *dir, const char *name)
{
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);

  return file_open (inode);
}

struct dir *
filesys_open_dir (struct dir *dir, const char *name)
{
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);

  return dir_open (inode);
}

bool
filesys_remove (struct dir *dir, const char *name)
{
  return dir != NULL && dir_remove (dir, name);
}

bool
filesys_create_file_at_root (const char *name, off_t initial_size) 
{
  struct dir *dir = dir_open_root ();
  bool success = filesys_create_file (dir, name, initial_size);
  dir_close (dir);

  return success;
}

struct file *
filesys_open_file_at_root (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct file *file = filesys_open_file (dir, name);
  dir_close (dir);

  return file;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove_at_root (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = filesys_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
