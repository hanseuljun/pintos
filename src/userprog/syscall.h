#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* For letting only one process at a time to access the file system.
   See 3.1.2 Using the File System. */
extern struct lock global_filesys_lock;

void syscall_init (void);
void syscall_exit(int status);

#endif /* userprog/syscall.h */
