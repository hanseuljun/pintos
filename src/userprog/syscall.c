#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/fs-cache.h"
#include "filesys/inode.h"
#include "filesys/path.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#ifdef VM
#include "vm/mmap_table.h"
#endif

#define FD_INFO_MAP_SIZE 128
/* Should not use numbers assigned to STDIN_FILENO (0) or STDOUT_FILENO (1)
   for fd values of files. */
#define FD_BASE 2

struct fd_info
{
  struct file *file;
  int pid;             /* pid of the process that opened this file. */
};

/* Map from fd values to file structs. */
static struct fd_info *fd_info_map[FD_INFO_MAP_SIZE];
static struct path *current_path;

struct lock global_filesys_lock;

static void syscall_handler (struct intr_frame *);
static void handle_exit (void *esp);
static int handle_exec (void *esp);
static int handle_wait (void *esp);
static bool handle_create (void *esp);
static bool handle_remove (void *esp);
static int handle_open (void *esp);
static int handle_filesize (void *esp);
static int handle_read (void *esp);
static int handle_write (void *esp);
static void handle_seek (void *esp);
static unsigned handle_tell (void *esp);
static void handle_close (void *esp);
#ifdef VM
static int handle_mmap (void *esp);
static void handle_munmap (void *esp);
#endif
static bool handle_chdir (void *esp);
static bool handle_mkdir (void *esp);
static bool handle_readdir (void *esp);
static int handle_inumber (void *esp);

static uint32_t get_argument (void *esp, size_t idx);
static int find_available_fd (void);
static bool is_uaddr_valid (const void *uaddr);
static bool is_fd_for_file (int fd);

typedef void dir_and_filename_func (struct dir *dir, const char *filename, void *aux);
static void run_dir_and_filename_func_with_path_str (const char *path_str, dir_and_filename_func *func, void *aux);

void
syscall_init (void) 
{
  current_path = path_create ("/");
  lock_init (&global_filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
syscall_exit (int status)
{
  struct thread *t = thread_current ();
  struct fd_info *fd_info;
  int i;

#ifdef VM
  if (lock_held_by_current_thread (&global_filesys_lock))
    lock_release (&global_filesys_lock);
#endif

  if (lock_held_by_current_thread (fs_cache_get_lock ()))
    lock_release (fs_cache_get_lock ());

  /* Pass status to parent. */
  int parent_tid = t->parent_tid;
  if (parent_tid != TID_ERROR)
    {
      enum intr_level old_level;
      old_level = intr_disable ();
      struct thread *parent = thread_find (parent_tid);
      if (parent)
        {
          struct thread_exit_info *exit_info = malloc (sizeof exit_info);
          exit_info->tid = t->tid;
          exit_info->exit_status = status;
          list_push_back (&parent->exit_info_list, &exit_info->elem);
        }
      intr_set_level (old_level);
    }

  /* Close all files that belongs to the exiting process. */
  lock_acquire (&global_filesys_lock);
  for (i = 0; i < FD_INFO_MAP_SIZE; i++)
    {
      fd_info = fd_info_map[i];
      if (fd_info != NULL && fd_info->pid == t->tid)
        {
          fd_info_map[i] = NULL;
          file_close (fd_info->file);
          free (fd_info);
        }
    }
  lock_release (&global_filesys_lock);

  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_exit ();
  NOT_REACHED ();
}

static void
syscall_handler (struct intr_frame *f) 
{
  int number;
  if (!is_uaddr_valid(f->esp))
    syscall_exit (-1);

  number = (int) get_argument (f->esp, 0);
  switch (number)
    {
      case SYS_HALT:
        shutdown_power_off ();
        NOT_REACHED ();
      case SYS_EXIT:
        handle_exit (f->esp);
        NOT_REACHED ();
      case SYS_EXEC:
        f->eax = handle_exec (f->esp);
        return;
      case SYS_WAIT:
        f->eax = handle_wait (f->esp);
        return;
      case SYS_CREATE:
        f->eax = handle_create (f->esp);
        return;
      case SYS_REMOVE:
        f->eax = handle_remove (f->esp);
        return;
      case SYS_OPEN:
        f->eax = handle_open (f->esp);
        return;
      case SYS_FILESIZE:
        f->eax = handle_filesize (f->esp);
        return;
      case SYS_READ:
        f->eax = handle_read (f->esp);
        return;
      case SYS_WRITE:
        f->eax = handle_write (f->esp);
        return;
      case SYS_SEEK:
        handle_seek (f->esp);
        return;
      case SYS_TELL:
        f->eax = handle_tell (f->esp);
        return;
      case SYS_CLOSE:
        handle_close (f->esp);
        return;
#ifdef VM
      case SYS_MMAP:
        f->eax = handle_mmap (f->esp);
        return;
      case SYS_MUNMAP:
        handle_munmap (f->esp);
        return;
      case SYS_CHDIR:
        f->eax = handle_chdir (f->esp);
        return;
      case SYS_MKDIR:
        f->eax = handle_mkdir (f->esp);
        return;
      case SYS_READDIR:
        f->eax = handle_readdir (f->esp);
        return;
      case SYS_INUMBER:
        f->eax = handle_inumber (f->esp);
        return;
#endif
    }
  
  printf ("Found a syscall with a number (%d) not implemented yet.\n", number);
  NOT_REACHED ();
}

static void
handle_exit (void *esp)
{
  int status = (int) get_argument(esp, 1);
  syscall_exit (status);
  NOT_REACHED ();
}

static int
handle_exec (void *esp)
{
  const char *cmd_line = (const char *) get_argument(esp, 1);
  /* Exit when cmd_line is pointing an invalid address. */
  if (!is_uaddr_valid (cmd_line))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  return process_execute (cmd_line);
}

static int
handle_wait (void *esp)
{
  int pid = (int) get_argument(esp, 1);
  return process_wait (pid);
}

struct handle_create_dir_and_filename_aux
{
  int initial_size;
  bool success;
};

static void handle_create_dir_and_filename_func (struct dir *dir, const char *filename, void *aux);

static bool
handle_create (void *esp)
{
  const char *path_str = (const char *) get_argument(esp, 1);
  unsigned initial_size = (unsigned) get_argument(esp, 2);

  /* Exit when path_str is pointing an invalid address. */
  if (!is_uaddr_valid (path_str))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  /* Exit when path_str is NULL. */
  if (path_str == NULL)
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  /* Fail when path_str is an empty string. */
  if (path_str[0] == '\0')
    {
      return false;
    }

  struct handle_create_dir_and_filename_aux aux;
  aux.initial_size = initial_size;
  run_dir_and_filename_func_with_path_str (path_str, handle_create_dir_and_filename_func, &aux);

  return aux.success;
}

static void
handle_create_dir_and_filename_func (struct dir *dir, const char *filename, void *aux)
{
  struct handle_create_dir_and_filename_aux *handle_create_dir_and_filename_aux = aux;
  handle_create_dir_and_filename_aux->success = filesys_create_file (dir, filename, handle_create_dir_and_filename_aux->initial_size);
}

static void handle_remove_dir_and_filename_func (struct dir *dir, const char *filename, void *aux);

static bool
handle_remove (void *esp)
{
  const char *path_str = (const char *) get_argument(esp, 1);
  /* Exit when path_str is pointing an invalid address. */
  if (!is_uaddr_valid (path_str))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  /* Exit when path_str is NULL. */
  if (path_str == NULL)
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  /* Fail when path_str is an empty string. */
  if (path_str[0] == '\0')
    {
      return false;
    }

  bool success;
  run_dir_and_filename_func_with_path_str (path_str, handle_remove_dir_and_filename_func, &success);

  return success;
}

static void
handle_remove_dir_and_filename_func (struct dir *dir, const char *filename, void *aux)
{
  printf ("handle_remove_dir_and_filename_func - 1, filename: %s\n", filename);
  bool *result = aux;
  *result = filesys_remove (dir, filename);
}

static void handle_open_dir_and_filename_func (struct dir *dir, const char *filename, void *aux);

static int
handle_open (void *esp)
{
  const char *path_str = (const char *) get_argument(esp, 1);
  struct file *file;
  struct fd_info *fd_info;

  /* Exit when path is pointing an invalid address. */
  if (!is_uaddr_valid (path_str))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  /* Exit when path is NULL. */
  if (path_str == NULL)
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  /* Fail when path is an empty string. */
  if (path_str[0] == '\0')
    {
      return -1;
    }

  run_dir_and_filename_func_with_path_str (path_str, handle_open_dir_and_filename_func, &file);

  if (file == NULL)
    return -1;

  int fd = find_available_fd ();

  fd_info = malloc (sizeof *fd_info);
  fd_info->file = file;
  fd_info->pid = thread_tid ();

  fd_info_map[fd - FD_BASE] = fd_info;
  return fd;
}

static void
handle_open_dir_and_filename_func (struct dir *dir, const char *filename, void *aux)
{
  struct file **result = aux;
  if (filename == NULL || (strcmp (filename, ".") == 0))
    *result = file_open (dir_get_inode (dir));
  else
    *result = filesys_open_file (dir, filename);
}

static int
handle_filesize (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  int filesize = file_length (fd_info->file);
  lock_release (&global_filesys_lock);
  return filesize;
}

static int
handle_read (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  void *buffer = (void *) get_argument(esp, 2);
  unsigned size = (unsigned) get_argument(esp, 3);

  if (!is_fd_for_file (fd))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  /* Exit when buffer is pointing an invalid address. */
  if (!is_uaddr_valid (buffer))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  int bytes_read = file_read (fd_info->file, buffer, size);
  lock_release (&global_filesys_lock);
  return bytes_read;
}

static int
handle_write (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  const void *buffer = (const void *)get_argument(esp, 2);
  unsigned size = (unsigned) get_argument(esp, 3);

  /* Exit when buffer is pointing an invalid address. */
  if (!is_uaddr_valid (buffer))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  if (fd == STDOUT_FILENO)
    return printf ((const char *) buffer);

  if (!is_fd_for_file (fd))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  int result;
  if (inode_isdir (file_get_inode (fd_info->file)))
    result = -1;
  else
    result = file_write (fd_info->file, buffer, size);
  lock_release (&global_filesys_lock);
  return result;
}

static void
handle_seek (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  unsigned position = (unsigned) get_argument(esp, 2);
  if (!is_fd_for_file (fd))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  file_seek (fd_info->file, position);
  lock_release (&global_filesys_lock);
}

static unsigned
handle_tell (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  if (!is_fd_for_file (fd))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  unsigned position = file_tell (fd_info->file);
  lock_release (&global_filesys_lock);
  return position;
}

static void
handle_close (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  if (!is_fd_for_file (fd))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  if (fd_info == NULL)
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  if (fd_info->pid != thread_tid ())
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  if (fd_info->file == NULL)
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }

  lock_acquire (&global_filesys_lock);
  file_close (fd_info->file);
  lock_release (&global_filesys_lock);
  fd_info_map[fd - FD_BASE] = NULL;
  free (fd_info);
}

#ifdef VM
static int
handle_mmap (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  void *addr = (void *) get_argument(esp, 2);

  if (pg_ofs (addr) != 0)
    return -1;
  if (addr == NULL)
    return -1;
  if (!is_fd_for_file (fd))
    return -1;
  if (mmap_table_contains (addr))
    return -1;

  uint32_t *pd = thread_current ()->pagedir;
  if (pagedir_get_page (pd, addr) != NULL)
    return -1;

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  int filesize = file_length (fd_info->file);
  lock_release (&global_filesys_lock);

  return mmap_table_add (fd_info->file, addr, filesize);
}

static void
handle_munmap (void *esp)
{
  int mapping = (int) get_argument(esp, 1);
  mmap_table_remove (mapping);
}
#endif

static bool
handle_chdir (void *esp)
{
  char *path_str = (char *) get_argument(esp, 1);

  path_push_back (current_path, path_str);

  return true;
}

static void handle_mkdir_dir_and_filename_func (struct dir *dir, const char *filename, void *aux);

static bool
handle_mkdir (void *esp)
{
  char *path_str = (char *) get_argument(esp, 1);
  /* Fail when path_str is an empty string. */
  if (path_str[0] == '\0')
    {
      return false;
    }

  bool success;
  run_dir_and_filename_func_with_path_str (path_str, handle_mkdir_dir_and_filename_func, &success);

  return success;
}

static bool
handle_readdir (void *esp)
{
  return false;
}

static int
handle_inumber (void *esp)
{
  int fd = (int) get_argument(esp, 1);

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  lock_acquire (&global_filesys_lock);
  int inumber = inode_get_inumber (file_get_inode (fd_info->file));
  lock_release (&global_filesys_lock);
  return inumber;
}

static void
handle_mkdir_dir_and_filename_func (struct dir *dir, const char *filename, void *aux)
{
  bool *result = aux;
  *result = filesys_create_dir (dir, filename);
}

static uint32_t
get_argument (void *esp, size_t idx)
{
  void *addr = esp + idx * sizeof(uint32_t);
  if (!is_uaddr_valid (addr))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  return *(uint32_t *) addr;
}

static int
find_available_fd (void)
{
  int i;
  for (i = 0; i < FD_INFO_MAP_SIZE; i++)
    {
      if (fd_info_map[i] == NULL)
        return i + FD_BASE;
    }

  printf ("Ran out of fd values.\n");
  NOT_REACHED ();
}

/* Check validity of uaddr assuming its size is 4 bytes. */
static bool
is_uaddr_valid (const void *uaddr)
{
#ifdef VM
  return is_user_vaddr (uaddr + 3);
#else
  uint32_t *pd;

  /* Examine the byte with the highest address. */
  if (!is_user_vaddr (uaddr + 3))
    return false;

  pd = thread_current ()->pagedir;
  /* Examine bytes at the both ends of the 4-byte address. */
  return pagedir_get_page (pd, uaddr) != NULL
      && pagedir_get_page (pd, uaddr + 3) != NULL;
#endif
}

static bool
is_fd_for_file (int fd)
{
  return (fd >= FD_BASE) && (fd < (FD_BASE + FD_INFO_MAP_SIZE));
}

static void
run_dir_and_filename_func_with_path_str (const char *path_str, dir_and_filename_func *func, void *aux)
{
  struct path *path;
  if (path_str[0] == '/')
    {
      path = path_create (path_str);
      path_sanitize (path);
    }
  else
  {
    path = path_copy (current_path);
    path_push_back (path, path_str);
    path_sanitize (path);
  }

  printf ("run_dir_and_filename_func_with_path_str - 1, path: %s\n", path_get_string (path));

  char *filename = path_pop_back (path);
  struct dir *dir = path_get_dir (path);

  lock_acquire (&global_filesys_lock);

  func (dir, filename, aux);
  dir_close (dir);

  lock_release (&global_filesys_lock);

  free (filename);
  path_release (path);
}
