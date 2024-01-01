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
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  int number = *((int *) f->esp);
  printf ("syscall number: %d\n", number);

  switch (number)
    {
      case SYS_WRITE:
        printf ("SYS_WRITE\n");
        int fd = ((int *) f->esp)[1];
        const void *buffer = (const void *) ((int *) f->esp)[2];
        unsigned size = ((int *) f->esp)[3];
        printf ("fd: %d\n", fd);
        // printf ("buffer: %p\n", buffer);
        // printf ("PHYS_BASE: %d\n", PHYS_BASE);
        printf ("size: %d\n", size);
        // printf ("buffer string: %s\n", (const char *) buffer);

        if (fd == STDOUT_FILENO)
          {
            printf ("%s\n", (const char *) buffer);
            f->eax = 0;
          }
        break;
    }

  
  printf ("thread name: %s\n", thread_name ());
  // thread_exit ();
}
