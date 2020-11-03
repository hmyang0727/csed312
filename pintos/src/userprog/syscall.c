#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
void halt (void);
void exit (int status);
int write (int fd, const void *buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void* esp = f->esp;
  int syscall_number = *(int*)esp;

  // hex_dump (esp, esp, 400, 1);

  // printf("Syscall Number: %d\n", syscall_number);
  // printf("ESP: %p\n", esp);
  // printf("Next: %p\n", esp + 4);

  switch(syscall_number) {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      exit (*(unsigned int*)(esp + 4));
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      write ((int)*(unsigned int*)(esp + 4), (void*)*(unsigned int*)(esp + 8), (unsigned)*(unsigned int*)(esp + 12));
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
  }
  // printf ("system call!\n");
  // thread_exit ();
}

void halt () {
  shutdown_power_off ();
}

void exit (int status) {
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

int write (int fd, const void *buffer, unsigned size) {
  if (fd == 1) {
    putbuf (buffer, size);
    return size;
  }
  return -1;
}