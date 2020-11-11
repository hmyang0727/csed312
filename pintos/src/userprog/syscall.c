#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdio.h>
#include <syscall-nr.h>

static void syscall_handler (struct intr_frame *);
void is_user_space (void *addr);
void halt (void);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_access_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void* esp = f->esp;
  int syscall_number = *(unsigned int*)esp;

  switch(syscall_number) {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      is_user_space (esp + 4);
      exit (*(unsigned int*)(esp + 4));
      break;
    case SYS_EXEC:
      is_user_space (esp + 4);
      is_user_space ((void*)*(unsigned int*)(esp + 4));
      f->eax = exec ((char*)*(unsigned int*)(esp + 4));
      break;
    case SYS_WAIT:
      is_user_space (esp + 4);
      f->eax = wait ((int)*(unsigned int*)(esp + 4));
      break;
    case SYS_CREATE:
      is_user_space (esp + 8);
      is_user_space ((void*)*(unsigned int*)(esp + 4));
      f->eax = create ((char*)*(unsigned int*)(esp + 4), (unsigned)*(unsigned int*)(esp + 8));
      break;
    case SYS_REMOVE:
      is_user_space (esp + 4);
      is_user_space ((void*)*(unsigned int*)(esp + 4));
      f->eax = remove ((char*)*(unsigned int*)(esp + 4));
      break;
    case SYS_OPEN:
      is_user_space (esp + 4);
      is_user_space ((void*)*(unsigned int*)(esp + 4));
      f->eax = open ((char*)*(unsigned int*)(esp + 4));
      break;
    case SYS_FILESIZE:
      is_user_space (esp + 4);
      f->eax = filesize ((int)*(unsigned int*)(esp + 4));
      break;
    case SYS_READ:
      is_user_space (esp + 12);
      is_user_space ((void*)*(unsigned int*)(esp + 8));
      is_user_space ((void*)*(unsigned int*)(esp + 8) + *(unsigned int*)(esp + 12));
      f->eax = read ((int)*(unsigned int*)(esp + 4), (void*)*(unsigned int*)(esp + 8), (unsigned)*(unsigned int*)(esp + 12));
      break;
    case SYS_WRITE:
      is_user_space (esp + 12);
      is_user_space ((void*)*(unsigned int*)(esp + 8));
      is_user_space ((void*)*(unsigned int*)(esp + 8) + *(unsigned int*)(esp + 12));
      f->eax = write ((int)*(unsigned int*)(esp + 4), (void*)*(unsigned int*)(esp + 8), (unsigned)*(unsigned int*)(esp + 12));
      break;
    case SYS_SEEK:
      is_user_space ((void*)*(unsigned int*)(esp + 8));
      seek ((int)*(unsigned int*)(esp + 4), (unsigned int)*(unsigned int*)(esp + 8));
      break;
    case SYS_TELL:
      is_user_space ((void*)*(unsigned int*)(esp + 4));
      f->eax = tell ((int)*(unsigned int*)(esp + 4));
      break;
    case SYS_CLOSE:
      is_user_space ((void*)*(unsigned int*)(esp + 4));
      close ((int)*(unsigned int*)(esp + 4));
      break;
  }
}

/* Check whether the given pointer is pointing the user space or not.
   If not, terminate the process. */
void is_user_space (void *addr) {
  if(!is_user_vaddr (addr)) {
    exit(-1);
  }
}

/* Terminate Pintos by calling shutdown_power_off (). */
void halt () {
  shutdown_power_off ();
}

/* Terminate the current user process with an exit message.
   Exit status will be returned to the kernel. 
   If parent process is waiting, that wait function will return this status. */
void exit (int status) {
  thread_current ()->exit_status = status;
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

pid_t exec (const char *cmd_line) {
  tid_t exec_tid;
  struct thread *exec_thread;
  struct list_elem *e;

  exec_tid = process_execute (cmd_line);

  for (e = list_begin (&thread_current ()->child_list); e != list_end (&thread_current ()->child_list); e = list_next (e)) {
    exec_thread = list_entry (e, struct thread, child_elem);
    if(exec_thread->tid == exec_tid) {
      sema_down (&exec_thread->load_sema);
      
      return exec_thread->load_success ? exec_tid : -1;
    }
  }

  return -1;
}

int wait (pid_t pid) {
  return process_wait (pid);
}

bool create (const char *file, unsigned initial_size) {
  if (file == NULL) {
    exit (-1);
  }
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  if (file == NULL) {
    exit (-1);
  }
  return filesys_remove (file);
}

int open (const char *file) {
  if (file == NULL) {
    return -1;
  }
  struct file *open_file = filesys_open (file);
  if (open_file == NULL) {
    return -1;
  }

  /* If file is running, opening file is not allowed. */
  if (!strcmp(thread_current()->name, file)) {
    file_deny_write(open_file);
  }

  thread_current ()->fd[thread_current ()->next_fd] = open_file;
  thread_current ()->next_fd++;
  return ((thread_current ()->next_fd) - 1);
}

int filesize (int fd) {
  if (thread_current ()->fd[fd] == NULL) {
    return -1;
  }

  return file_length (thread_current ()->fd[fd]);
}

int read (int fd, void *buffer, unsigned size) {
  int i, retval;
  lock_acquire (&file_access_lock);
  /* Standard input. */
  if (fd == 0) {
    for (i = 0; i < size; i++) {
      *((uint8_t*)buffer + i) = input_getc ();
    }
    lock_release (&file_access_lock);
    return size;
  }

  if (thread_current ()->fd[fd] == NULL) {
    lock_release (&file_access_lock);
    return -1;
  }

  retval = file_read (thread_current ()->fd[fd], buffer, size);
  lock_release (&file_access_lock);
  return retval;
}

int write (int fd, const void *buffer, unsigned size) {
  int retval;
  lock_acquire (&file_access_lock);
  /* Standard output. */
  if (fd == 1) {
    putbuf (buffer, size);
    lock_release (&file_access_lock);
    return size;
  }

  if (thread_current ()->fd[fd] == NULL) {
    lock_release (&file_access_lock);
    return -1;
  }

  retval = file_write (thread_current ()->fd[fd], buffer, size);
  lock_release (&file_access_lock);
  return retval;
}

void seek (int fd, unsigned position) {
  lock_acquire (&file_access_lock);
  if (thread_current ()->fd[fd] == NULL) {
    lock_release (&file_access_lock);
    return;
  }

  file_seek (thread_current ()->fd[fd], position);
  lock_release (&file_access_lock);
  return;
}

unsigned tell (int fd) {
  int retval;
  lock_acquire (&file_access_lock);
  if (thread_current ()->fd[fd] == NULL) {
    lock_release (&file_access_lock);
    return -1;
  }

  retval = file_tell (thread_current ()->fd[fd]);
  lock_release (&file_access_lock);
  return retval;
}

void close (int fd) {
  lock_acquire (&file_access_lock);
  if (thread_current ()->fd[fd] == NULL) {
    lock_release (&file_access_lock);
    return;
  }

  file_close (thread_current ()->fd[fd]);
  thread_current ()->fd[fd] = NULL;
  lock_release (&file_access_lock);
  return;
}