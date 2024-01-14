#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exit (int status);
static uint32_t get_argument (void *esp, size_t idx);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  struct thread *cur = thread_current ();
  uint32_t *pd = cur->pagedir;
  // printf ("thread name: %s\n", cur->name);
  // printf ("pd: %p\n", pd);
  // printf ("init_page_dir: %p\n", init_page_dir);
  // TODO: Improve the memory access checking happening here. 
  // printf ("esp: %p\n", f->esp);
  /* 0x08048000 is the starting address of the code segment.
     It is safe to assume the stack pointer is pointing an invalid
     address if it is pointing below here.
     See 3.1.4.1 Typical Memory Layout for the origin of 0x08048000. */
  if ((size_t)f->esp < 0x08048000)
    syscall_exit (-1);
  int number = (int) get_argument (f->esp, 0);

  switch (number)
    {
      case SYS_EXIT:
        int status = (int) get_argument(f->esp, 1);
        syscall_exit (status);
        NOT_REACHED ();
        break;
      case SYS_CREATE:
        const char *file = (const char *) get_argument(f->esp, 1);
        if (file == NULL)
          {
            syscall_exit (-1);
            NOT_REACHED ();
          }
        // TODO: Create file. Currently faking success.
        f->eax = 0;
        break;
      case SYS_WRITE:
        int fd = (int) get_argument(f->esp, 1);
        const void *buffer = (const void *)get_argument(f->esp, 2);
        // printf ("buffer: %p\n", buffer);
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

static uint32_t get_argument (void *esp, size_t idx)
{
  uint32_t *addr = ((uint32_t *) esp) + idx;
  // printf ("get_argument, addr: %p\n", addr);
  if (!is_user_vaddr (addr))
    {
      syscall_exit (-1);
      NOT_REACHED ();
    }
  return *addr;
}
