#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/stdio.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

struct lock filesys_lock;
void* esp;

static void syscall_handler(struct intr_frame *);

static void check_vaddr(const void *);

static void syscall_halt(void);
static pid_t syscall_exec(const char *);
static int syscall_wait(pid_t);
static bool syscall_create(const char *, unsigned);
static bool syscall_remove(const char *);
static int syscall_open(const char *);
static int syscall_filesize(int);
static int syscall_read(int, void *, unsigned);
static int syscall_write(int, const void *, unsigned);
static void syscall_seek(int, unsigned);
static unsigned syscall_tell(int);
static mapid_t syscall_mmap (int, void*);

struct mmap_table_entry* find_mmap_table_entry(struct thread*, mapid_t);

/* Registers the system call interrupt handler. */
void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesys_lock);
}

/* Pops the system call number and handles system call
   according to it. */
static void
syscall_handler(struct intr_frame *f)
{
    // void *esp = f->esp;
    esp = f->esp;
    int syscall_num;

    check_vaddr(esp);
    check_vaddr(esp + sizeof(uintptr_t) - 1);
    syscall_num = *(int *)esp;

    switch (syscall_num)
    {
    case SYS_HALT:
    {
        syscall_halt();
        NOT_REACHED();
    }
    case SYS_EXIT:
    {
        int status;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        status = *(int *)(esp + sizeof(uintptr_t));

        syscall_exit(status);
        NOT_REACHED();
    }
    case SYS_EXEC:
    {
        char *cmd_line;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        cmd_line = *(char **)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_exec(cmd_line);
        break;
    }
    case SYS_WAIT:
    {
        pid_t pid;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        pid = *(pid_t *)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_wait(pid);
        break;
    }
    case SYS_CREATE:
    {
        char *file;
        unsigned initial_size;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 3 * sizeof(uintptr_t) - 1);
        file = *(char **)(esp + sizeof(uintptr_t));
        initial_size = *(unsigned *)(esp + 2 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_create(file, initial_size);
        break;
    }
    case SYS_REMOVE:
    {
        char *file;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        file = *(char **)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_remove(file);
        break;
    }
    case SYS_OPEN:
    {
        char *file;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        file = *(char **)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_open(file);
        break;
    }
    case SYS_FILESIZE:
    {
        int fd;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_filesize(fd);
        break;
    }
    case SYS_READ:
    {
        int fd;
        void *buffer;
        unsigned size;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 4 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        buffer = *(void **)(esp + 2 * sizeof(uintptr_t));
        size = *(unsigned *)(esp + 3 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_read(fd, buffer, size);
        break;
    }
    case SYS_WRITE:
    {
        int fd;
        void *buffer;
        unsigned size;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 4 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        buffer = *(void **)(esp + 2 * sizeof(uintptr_t));
        size = *(unsigned *)(esp + 3 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_write(fd, buffer, size);
        break;
    }
    case SYS_SEEK:
    {
        int fd;
        unsigned position;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 3 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        position = *(unsigned *)(esp + 2 * sizeof(uintptr_t));

        syscall_seek(fd, position);
        break;
    }
    case SYS_TELL:
    {
        int fd;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_tell(fd);
        break;
    }
    case SYS_CLOSE:
    {
        int fd;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));

        syscall_close(fd);
        break;
    }
    case SYS_MMAP:
    {
        int fd;
        void* addr;

        check_vaddr (esp + sizeof (uintptr_t));
        check_vaddr (esp + 3 * sizeof (uintptr_t) - 1);
        fd = *(int*)(esp + sizeof (uintptr_t));
        addr = *(void **)(esp + 2 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_mmap (fd, addr);
        break;
    }
    case SYS_MUNMAP:
    {
        mapid_t mapping;

        check_vaddr (esp + sizeof (uintptr_t));
        check_vaddr (esp + 2 * sizeof (uintptr_t) - 1);
        mapping = *(mapid_t*)(esp + sizeof (uintptr_t));

        syscall_munmap (mapping);
        break;
    }
    default:
        syscall_exit(-1);
    }
}

/* Checks user-provided virtual address. If it is
   invalid, terminates the current process. */
static void
check_vaddr(const void *vaddr)
{
    if (!vaddr || !is_user_vaddr(vaddr))
        syscall_exit(-1);
}

struct lock *syscall_get_filesys_lock(void)
{
    return &filesys_lock;
}

/* Handles halt() system call. */
static void syscall_halt(void)
{
    shutdown_power_off();
}

/* Handles exit() system call. */
void syscall_exit(int status)
{
    struct process *pcb = thread_get_pcb();

    pcb->exit_status = status;
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_exit();
}

/* Handles exec() system call. */
static pid_t syscall_exec(const char *cmd_line)
{
    pid_t pid;
    struct process *child;
    int i;

    check_vaddr(cmd_line);
    for (i = 0; *(cmd_line + i); i++)
        check_vaddr(cmd_line + i + 1);

    pid = process_execute(cmd_line);
    child = process_get_child(pid);

    if (!child || !child->is_loaded)
        return PID_ERROR;

    return pid;
}

/* Handles wait() system call. */
static int syscall_wait(pid_t pid)
{
    return process_wait(pid);
}

/* Handles create() system call. */
static bool syscall_create(const char *file, unsigned initial_size)
{
    bool success;
    int i;

    check_vaddr(file);
    for (i = 0; *(file + i); i++)
        check_vaddr(file + i + 1);

    lock_acquire(&filesys_lock);
    success = filesys_create(file, (off_t)initial_size);
    lock_release(&filesys_lock);

    return success;
}

/* Handles remove() system call. */
static bool syscall_remove(const char *file)
{
    bool success;
    int i;

    check_vaddr(file);
    for (i = 0; *(file + i); i++)
        check_vaddr(file + i + 1);

    lock_acquire(&filesys_lock);
    success = filesys_remove(file);
    lock_release(&filesys_lock);

    return success;
}

/* Handles open() system call. */
static int syscall_open(const char *file)
{
    struct file_descriptor_entry *fde;
    struct file *new_file;
    int i;

    check_vaddr(file);
    for (i = 0; *(file + i); i++)
        check_vaddr(file + i + 1);

    fde = palloc_get_page(0);
    if (!fde)
        return -1;

    lock_acquire(&filesys_lock);

    new_file = filesys_open(file);
    if (!new_file)
    {
        palloc_free_page(fde);
        lock_release(&filesys_lock);

        return -1;
    }

    fde->fd = thread_get_next_fd();
    fde->file = new_file;
    list_push_back(thread_get_fdt(), &fde->fdtelem);

    lock_release(&filesys_lock);

    return fde->fd;
}

/* Handles filesize() system call. */
static int syscall_filesize(int fd)
{
    struct file_descriptor_entry *fde = process_get_fde(fd);
    int filesize;

    if (!fde)
        return -1;

    lock_acquire(&filesys_lock);
    filesize = file_length(fde->file);
    lock_release(&filesys_lock);

    return filesize;
}

/* Handles read() system call. */
static int syscall_read(int fd, void *buffer, unsigned size)
{
    struct file_descriptor_entry *fde;
    int bytes_read, i;

    for (i = 0; i < size; i++) {
        check_vaddr(buffer + i);
    }

    if (fd == 0)
    {
        unsigned i;

        for (i = 0; i < size; i++)
            *(uint8_t *)(buffer + i) = input_getc();

        return size;
    }

    fde = process_get_fde(fd);
    if (!fde)
        return -1;

    lock_acquire(&filesys_lock);
    bytes_read = (int)file_read(fde->file, buffer, (off_t)size);
    lock_release(&filesys_lock);

    return bytes_read;
}

/* Handles write() system call. */
static int syscall_write(int fd, const void *buffer, unsigned size)
{
    struct file_descriptor_entry *fde;
    int bytes_written, i;

    for (i = 0; i < size; i++)
        check_vaddr(buffer + i);

    if (fd == 1)
    {
        putbuf((const char *)buffer, (size_t)size);

        return size;
    }

    fde = process_get_fde(fd);
    if (!fde)
        return -1;

    lock_acquire(&filesys_lock);
    bytes_written = (int)file_write(fde->file, buffer, (off_t)size);
    lock_release(&filesys_lock);

    return bytes_written;
}

/* Handles seek() system call. */
static void syscall_seek(int fd, unsigned position)
{
    struct file_descriptor_entry *fde = process_get_fde(fd);

    if (!fde)
        return;

    lock_acquire(&filesys_lock);
    file_seek(fde->file, (off_t)position);
    lock_release(&filesys_lock);
}

/* Handles tell() system call. */
static unsigned syscall_tell(int fd)
{
    struct file_descriptor_entry *fde = process_get_fde(fd);
    unsigned pos;

    if (!fde)
        return -1;

    lock_acquire(&filesys_lock);
    pos = (unsigned)file_tell(fde->file);
    lock_release(&filesys_lock);

    return pos;
}

/* Handles close() system call. */
void syscall_close(int fd)
{
    struct file_descriptor_entry *fde = process_get_fde(fd);

    if (!fde)
        return;

    lock_acquire(&filesys_lock);
    file_close(fde->file);
    list_remove(&fde->fdtelem);
    palloc_free_page(fde);
    lock_release(&filesys_lock);
}

mapid_t syscall_mmap (int fd, void* addr) {
    struct file_descriptor_entry* fde;
    off_t len, position;
    struct supplemental_page_table_entry* spte, hash_finder;
    struct hash_elem* found_elem;
    struct thread* t = thread_current ();
    struct file* fp;
    struct mmap_table_entry* mte;
    uint32_t read_bytes, zero_bytes;

    /* Mapping stdin or stdout? Is addr valid? */
    if (fd == 0 || fd == 1 || addr == NULL || (uintptr_t)addr & 0xfff) {
        return -1;
    }

    /* Is stack area? */
    if (PHYS_BASE - 0x800000 <= addr) {
        return -1;
    }

    fde = process_get_fde (fd);

    /* No such file descriptor? */
    if (!fde) {
        return -1;
    }

    lock_acquire (&filesys_lock);
    len = file_length (fde->file);
    lock_release (&filesys_lock);

    /* Zero length? */
    if (len == 0) {
        return -1;
    }

    /* Already mapped? */
    for (position = 0; position < len; position += PGSIZE) {
        spte = find_spte (t, addr + position);

        if (spte) {
            return -1;
        }
    }

    lock_acquire (&filesys_lock);
    fp = file_reopen (fde->file);
    lock_release (&filesys_lock);

    mte = (struct mmap_table_entry*)malloc (sizeof (struct mmap_table_entry));

    ASSERT (mte != NULL);

    mte->mapid = t->max_mapid++;
    mte->file = fp;
    mte->vaddr = addr;

    for (position = 0; position < len; position += PGSIZE) {
        read_bytes = len - position < PGSIZE ? len - position : PGSIZE;
        zero_bytes = PGSIZE - read_bytes;
        if (!insert_unmapped_spte (t, fp, position, addr + position, NULL, read_bytes, zero_bytes, true, 0, true)) {
            return -1;
        }
    }

    list_push_back (&t->mmap_table, &mte->elem);

    return mte->mapid;
}

void syscall_munmap (mapid_t mapping) {
    off_t len, position;
    struct thread* t = thread_current ();
    struct mmap_table_entry* mte;
    struct hash_elem* elem;
    struct supplemental_page_table_entry* spte_finder, *spte;
    void* vaddr;

    mte = find_mmap_table_entry(t, mapping);

    if(mte == NULL) { return; }

    lock_acquire(&filesys_lock);
    len = file_length(mte->file);
    for(position = 0; position < len; position += PGSIZE) {
        /* Different action comparing with dirty bit and status. 
         * Now in physical memory or swap disk?
         * Is dirty? Save its information to disk. */
        // 1. delete supplemental page table entry and save it to spte variable.
        // 2. if (pagedir_is_dirty && status == 1) need to write back.
        // 3. move file pos to position using file_seek.
        // 4. using spte->kpage, call file_write function.
        vaddr = mte->vaddr + position;
        spte_finder = find_spte (t, vaddr);
        elem = hash_delete (&t->supplemental_page_table, &spte_finder->elem);
        spte = hash_entry (elem, struct supplemental_page_table_entry, elem);
        if (pagedir_is_dirty (t->pagedir, vaddr) && spte->status == 1) {
            file_seek (spte->file, position);
            file_write (spte->file, spte->kpage, spte->read_bytes);
        }
        free (spte);
    }

    file_close (mte->file);
    list_remove (&mte->elem);
    lock_release(&filesys_lock);

    return;
}

struct mmap_table_entry* find_mmap_table_entry(struct thread* t, mapid_t mapping) {
    struct mmap_table_entry* mte;
    struct list_elem *e;

    ASSERT(t != NULL);

    for (e = list_begin(&t->mmap_table); e != list_end(&t->mmap_table); e = list_next(e))
    {
        mte = list_entry(e, struct mmap_table_entry, elem);
        if(mte->mapid == mapping) { 
            return mte;
        }
    }

    return NULL;
}