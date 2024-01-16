#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

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

static void syscall_handler (struct intr_frame *);
static void syscall_exit (void *esp);
static int syscall_exec (void *esp);
static int syscall_wait (void *esp);
static bool syscall_create (void *esp);
static int syscall_open (void *esp);
static int syscall_filesize (void *esp);
static int syscall_read (void *esp);
static int syscall_write (void *esp);
static void syscall_seek (void *esp);
static void syscall_close (void *esp);
static uint32_t get_argument (void *esp, size_t idx);
static void exit(int status);
static int find_available_fd (void);
static bool is_uaddr_valid (const void *uaddr);
static bool is_uaddr_in_page (uint32_t *pd, const void *uaddr);
static bool is_fd_for_file (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int number;
  if (!is_uaddr_valid(f->esp))
    exit (-1);

  number = (int) get_argument (f->esp, 0);
  switch (number)
    {
      case SYS_HALT:
        shutdown_power_off ();
        NOT_REACHED ();
      case SYS_EXIT:
        syscall_exit (f->esp);
        NOT_REACHED ();
      case SYS_EXEC:
        f->eax = syscall_exec(f->esp);
        return;
      case SYS_WAIT:
        f->eax = syscall_wait(f->esp);
        return;
      case SYS_CREATE:
        f->eax = syscall_create (f->esp);
        return;
      case SYS_OPEN:
        f->eax = syscall_open (f->esp);
        return;
      case SYS_FILESIZE:
        f->eax = syscall_filesize (f->esp);
        return;
      case SYS_READ:
        f->eax = syscall_read (f->esp);
        return;
      case SYS_WRITE:
        f->eax = syscall_write (f->esp);
        return;
      case SYS_SEEK:
        syscall_seek (f->esp);
        return;
      case SYS_CLOSE:
        syscall_close (f->esp);
        return;
    }
  
  printf ("Found a syscall with a number (%d) not implemented yet.\n", number);
  NOT_REACHED ();
}

static void syscall_exit (void *esp)
{
  int status = (int) get_argument(esp, 1);
  exit (status);
  NOT_REACHED ();
}

static int syscall_exec (void *esp)
{
  const char *cmd_line = (const char *) get_argument(esp, 1);
  /* Exit when cmd_line is pointing an invalid address. */
  if (!is_uaddr_valid (cmd_line))
    {
      exit (-1);
      NOT_REACHED ();
    }
  return process_execute (cmd_line);
}

static int syscall_wait (void *esp)
{
  int pid = (int) get_argument(esp, 1);
  return process_wait (pid);
}

static bool syscall_create (void *esp)
{
  const char *file_name = (const char *) get_argument(esp, 1);
  unsigned initial_size = (unsigned) get_argument(esp, 2);

  /* Exit when file_name is pointing an invalid address. */
  if (!is_uaddr_valid (file_name))
    {
      exit (-1);
      NOT_REACHED ();
    }
  /* Exit when file_name is NULL. */
  if (file_name == NULL)
    {
      exit (-1);
      NOT_REACHED ();
    }
  /* Fail when file_name is an empty string. */
  if (file_name[0] == '\0')
    {
      return false;
    }

  return filesys_create (file_name, initial_size);
}

static int syscall_open (void *esp)
{
  const char *file_name = (const char *) get_argument(esp, 1);
  struct file *file;
  struct fd_info *fd_info;

  /* Exit when file_name is pointing an invalid address. */
  if (!is_uaddr_valid (file_name))
    {
      exit (-1);
      NOT_REACHED ();
    }
  /* Exit when file_name is NULL. */
  if (file_name == NULL)
    {
      exit (-1);
      NOT_REACHED ();
    }

  file = filesys_open (file_name);
  if (file == NULL)
    return -1;

  int fd = find_available_fd ();

  fd_info = malloc (sizeof *fd_info);
  fd_info->file = file;
  fd_info->pid = thread_tid ();

  fd_info_map[fd - FD_BASE] = fd_info;
  return fd;
}

static int syscall_filesize (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  return file_length (fd_info->file);
}

static int syscall_read (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  void *buffer = (void *) get_argument(esp, 2);
  unsigned size = (unsigned) get_argument(esp, 3);

  if (!is_fd_for_file (fd))
    {
      exit (-1);
      NOT_REACHED ();
    }

  /* Exit when buffer is pointing an invalid address. */
  if (!is_uaddr_valid (buffer))
    {
      exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  return file_read (fd_info->file, buffer, size);
}

static int syscall_write (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  const void *buffer = (const void *)get_argument(esp, 2);
  unsigned size = (unsigned) get_argument(esp, 3);

  /* Exit when buffer is pointing an invalid address. */
  if (!is_uaddr_valid (buffer))
    {
      exit (-1);
      NOT_REACHED ();
    }

  if (fd == STDOUT_FILENO)
    return printf ((const char *) buffer);

  if (!is_fd_for_file (fd))
    {
      exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  return file_write (fd_info->file, buffer, size);
}

static void syscall_seek (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  unsigned position = (unsigned) get_argument(esp, 2);
  if (!is_fd_for_file (fd))
    {
      exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  file_seek (fd_info->file, position);
}

static void syscall_close (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  if (!is_fd_for_file (fd))
    {
      exit (-1);
      NOT_REACHED ();
    }

  struct fd_info *fd_info = fd_info_map[fd - FD_BASE];
  if (fd_info == NULL)
    {
      exit (-1);
      NOT_REACHED ();
    }
  if (fd_info->pid != thread_tid ())
    {
      exit (-1);
      NOT_REACHED ();
    }
  if (fd_info->file == NULL)
    {
      exit (-1);
      NOT_REACHED ();
    }

  file_close (fd_info->file);
  fd_info_map[fd - FD_BASE] = NULL;
  free (fd_info);
}

static uint32_t get_argument (void *esp, size_t idx)
{
  void *addr = esp + idx * sizeof(uint32_t);
  if (!is_uaddr_valid (addr))
    {
      exit (-1);
      NOT_REACHED ();
    }
  return *(uint32_t *) addr;
}

static void exit (int status)
{
  struct thread *t = thread_current ();
  struct fd_info *fd_info;
  int i;

  /* Pass status to the waiter. */
  if (t->exit_status_waiter)
    *t->exit_status_waiter = status;

  /* Close all files that belongs to the exiting process. */
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

  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_exit ();
  NOT_REACHED ();
}

static int find_available_fd (void)
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
static bool is_uaddr_valid (const void *uaddr)
{
  struct thread *t;
  uint32_t *pd;

  /* Examine the byte with the highest address. */
  if (!is_user_vaddr (uaddr + 3))
    return false;

  t = thread_current ();
  pd = t->pagedir;
  /* Examine bytes at the both ends of the 4-byte address. */
  return is_uaddr_in_page (pd, uaddr) && is_uaddr_in_page (pd, uaddr + 3);
}

static bool is_uaddr_in_page (uint32_t *pd, const void *uaddr)
{
  return pagedir_get_page (pd, uaddr) != NULL;
}

static bool is_fd_for_file (int fd)
{
  return (fd >= FD_BASE) && (fd < (FD_BASE + FD_INFO_MAP_SIZE));
}
