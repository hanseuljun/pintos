#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#define FILE_MAP_SIZE 128
/* Should not use numbers assigned to STDIN_FILENO (0) or STDOUT_FILENO (1)
   for fd values of files. */
#define FD_BASE 2

/* Map from fd values to file structs. */
static struct file *file_map[FILE_MAP_SIZE];

static void syscall_handler (struct intr_frame *);
static void syscall_exit (void *esp);
static bool syscall_create (void *esp);
static int syscall_open (void *esp);
static void syscall_write (void *esp);
static uint32_t get_argument (void *esp, size_t idx);
static void exit(int status);
static int find_available_fd (void);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int number;
  /* 0x08048000 is the starting address of the code segment.
     It is safe to assume the stack pointer is pointing an invalid
     address if it is pointing below here.
     See 3.1.4.1 Typical Memory Layout for the origin of 0x08048000. */
  if ((size_t)f->esp < 0x08048000)
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
      case SYS_CREATE:
        f->eax = syscall_create (f->esp);
        return;
      case SYS_OPEN:
        f->eax = syscall_open (f->esp);
        return;
      case SYS_WRITE:
        syscall_write (f->esp);
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

static bool syscall_create (void *esp)
{
  const char *file_name = (const char *) get_argument(esp, 1);
  unsigned *initial_size = (unsigned) get_argument(esp, 2);
  struct thread *t = thread_current ();

  /* Exit when file_name is pointing an invalid address. */
  if (pagedir_get_page (t->pagedir, file_name) == NULL)
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
  struct thread *t = thread_current ();
  struct file *file;

  /* Exit when file_name is pointing an invalid address. */
  if (pagedir_get_page (t->pagedir, file_name) == NULL)
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
  file_map[fd - FD_BASE] = file;
  return fd;
}

static void syscall_write (void *esp)
{
  int fd = (int) get_argument(esp, 1);
  const void *buffer = (const void *)get_argument(esp, 2);
  if (fd == STDOUT_FILENO)
    {
      printf ((const char *) buffer);
    }
  // TOOD: Implement other cases.
}

static uint32_t get_argument (void *esp, size_t idx)
{
  uint32_t *addr = ((uint32_t *) esp) + idx;
  if (!is_user_vaddr (addr))
    {
      exit (-1);
      NOT_REACHED ();
    }
  return *addr;
}

static void exit (int status)
{
  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_exit ();
  NOT_REACHED ();
}

static int find_available_fd (void)
{
  int i;
  for (i = 0; i < FILE_MAP_SIZE; i++)
    {
      if (file_map[i] == NULL)
        return i + FD_BASE;
    }

  printf ("Ran out of fd values.\n");
  NOT_REACHED ();
}
