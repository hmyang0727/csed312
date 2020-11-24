#include <list.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

static struct list frame_table;         /* Frame table. */
static struct lock frame_table_lock;    /* Lock for frame table. */

/* Frame table entry. */
struct frame_table_entry {
    void* upage;
    void* kpage;
    struct thread* owner;
    struct list_elem elem;
};