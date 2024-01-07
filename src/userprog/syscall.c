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
  printf ("system call!\n");
  thread_exit ();

  /*
  int *arguments = f->esp;
  int number = arguments[0];
  printf ("syscall number: %d\n", number);
  printf ("thread name: %s\n", thread_name ());

  switch (number)
    {
      case SYS_EXIT:
        thread_exit ();
        NOT_REACHED ();
      case SYS_WRITE:
        int fd = arguments[1];
        const void *buffer = (const void *) arguments[2];
        // unsigned size = (unsigned) arguments[3];
        // printf ("fd: %d\n", fd);
        // printf ("buffer: %p\n", buffer);
        // printf ("PHYS_BASE: %d\n", PHYS_BASE);
        // printf ("size: %d\n", size);
        // printf ("buffer string: %s\n", (const char *) buffer);

        if (fd == STDOUT_FILENO)
          {
            printf ((const char *) buffer);
          }
        break;
    }
    */
}
