#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // printf ("system call!\n");
  int *arguments = f->esp;
  int number = arguments[0];
  // printf ("syscall number: %d\n", number);
  // printf ("thread name: %s\n", thread_name ());

  switch (number)
    {
      case SYS_EXIT:
        int status = arguments[1];
        printf ("%s: exit(%d)\n", thread_name (), status);
        thread_exit ();
        NOT_REACHED ();
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
