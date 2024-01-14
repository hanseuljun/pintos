#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exit (int status);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // TODO: Improve the memory access checking happening here. 
  // printf ("esp: %p\n", f->esp);
  /* 0x08084000 is the starting point of the code segment,
     which is a very safe lower bound for detecting
     bad memory access. */
  if ((size_t)f->esp < 0x08084000)
    syscall_exit (-1);
  /* 0xbffffffc is where the program arguments should be at.
     0xbffffffc is chosen as a safe upper bound for detecting
     bad memory access. */
  if ((size_t)f->esp >= 0xbffffffc)
    syscall_exit (-1);
  int *arguments = f->esp;
  int number = arguments[0];

  switch (number)
    {
      case SYS_EXIT:
        int status = arguments[1];
        syscall_exit (status);
        NOT_REACHED ();
        break;
      case SYS_CREATE:
        const char *file = (const char *)arguments[1];
        if (file == NULL)
          {
            syscall_exit (-1);
            NOT_REACHED ();
          }
        // TODO: Create file. Currently faking success.
        f->eax = 0;
        break;
      case SYS_WRITE:
        int fd = arguments[1];
        const void *buffer = (const void *) arguments[2];
        if (fd == STDOUT_FILENO)
          {
            printf ((const char *) buffer);
          }
        break;
    }
}

static void syscall_exit (int status)
{
  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_exit ();
  NOT_REACHED ();
}
