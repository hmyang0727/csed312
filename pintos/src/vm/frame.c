#include <list.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

static struct list frame_table;         /* Frame table. */
static struct lock frame_table_lock;    /* Lock for frame table. */

/* Frame table entry. */
struct frame_table_entry {
    void* upage;   /* Virtual address. */
    void* kpage;   /* Physical address. */
    struct thread* owner;   /* Which process is owning this frame? */
    struct list_elem elem;
};

void frame_init () {
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

void* alloc_frame_entry (enum palloc_flags flags, uint8_t* upage) {
    // int is_user = flags & PAL_USER;
    // int is_zero = flags & PAL_ZERO;
    void* frame;
    struct frame_table_entry* fte = malloc(sizeof(struct frame_table_entry) * 1);

    ASSERT (fte != NULL);

    frame = palloc_get_page (flags);
    if(!frame) {
        free (fte);
        return NULL;
    }

    fte->owner = thread_current ();
    fte->kpage = frame;
    fte->upage = upage;

    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &fte->elem);
    lock_release (&frame_table_lock);

    return frame;
}

void free_frame_entry (void* kpage) {
    struct frame_table_entry* target_fte;
    struct list_elem* e;

    lock_acquire (&frame_table_lock);
    for(e = list_head (&frame_table); e != list_end (&frame_table); e = list_next(e)) {
        target_fte = list_entry (e, struct frame_table_entry, elem);
        if (target_fte->kpage == kpage) {
            list_remove (e);
            free (target_fte);
            break;
        }
    }
    lock_release (&frame_table_lock);

    palloc_free_page (kpage);
}