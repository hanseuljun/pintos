#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exit (void *esp);
static bool syscall_create (void *esp);
static int syscall_open (void *esp);
static void syscall_write (void *esp);
static uint32_t get_argument (void *esp, size_t idx);
static void exit(int status);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // printf ("esp: %p\n", f->esp);
  // printf ("&_end_bss: %p\n", &_end_bss);
  /* 0x08048000 is the starting address of the code segment.
     It is safe to assume the stack pointer is pointing an invalid
     address if it is pointing below here.
     See 3.1.4.1 Typical Memory Layout for the origin of 0x08048000. */
  if ((size_t)f->esp < 0x08048000)
    exit (-1);
  int number = (int) get_argument (f->esp, 0);
  // printf ("number: %d\n", number);

  switch (number)
    {
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
  
  printf ("Found syscall with a number (%d) not implemented yet.\n", number);
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
  struct file *file = filesys_open (file_name);
  if (file == NULL)
    return -1;
  return file;
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
  // printf ("get_argument, addr: %p\n", addr);
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
