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

static struct frame_table_entry* clock_pointer;

static struct frame_table_entry* get_victim ();

void frame_init () {
    list_init (&frame_table);
    lock_init (&frame_table_lock);
    clock_pointer = NULL;
}

void* alloc_frame_entry (enum palloc_flags flags, uint8_t* upage) {
    void* frame;
    struct frame_table_entry* fte, *victim;
    struct supplemental_page_table_entry* victim_spte;

    fte = malloc(sizeof(struct frame_table_entry));

    ASSERT (fte != NULL);

    frame = palloc_get_page (flags);
    if(!frame) {
        /* Eviction. */
        lock_acquire (&frame_table_lock);
        victim = get_victim ();
        list_remove (&victim->elem);
        list_push_back (&frame_table, &victim->elem);

        // 1. is_mmap and is_dirty => write back.
        //    !is_mmap and is_dirty => go to the swap disk.
        //    !is_dirty && upage is in stack area => go to the swap disk.
        //    else => Change its spte status to 0(not loaded) and ignore.
        // 2. Change victim's supplemental page table entry.
        // 3. Change victim information. (upage and owner) Below!

        victim_spte = find_spte (victim->upage);

        /* Page is dirty and memory-mapped file: Write back. */
        if (victim_spte->is_mmap && pagedir_is_dirty (victim->owner->pagedir, victim->upage)) {
            file_seek (victim_spte->file, victim_spte->ofs);
            file_write (victim_spte->file, victim_spte->kpage, victim_spte->read_bytes);
        }
        /* Page is dirty and originated from file, not mmap file: Swap. */
        else if (!victim_spte->is_mmap && pagedir_is_dirty (victim->owner->pagedir, victim->upage)) {
            // swapppppppppppppppppp
        }
        /* Page is not dirty and vaddr is in the stack area: Swap. */
        else if (!pagedir_is_dirty (victim->owner->pagedir, victim->upage) && PHYS_BASE - 0x800000 <= victim->upage) {
            // swappppppppppppppppppp
        }
        /* Ignore. */
        else {
            // Yay
        }

        // Change victim's spte information.
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
            if (clock_pointer == target_fte) {
                clock_pointer = NULL;
            }
            list_remove (e);
            free (target_fte);
            break;
        }
    }
    lock_release (&frame_table_lock);

    palloc_free_page (kpage);
}

/* Select victim based on Clock algorithm. */
static struct frame_table_entry* get_victim () {
    struct thread* t;
    struct frame_table_entry* victim;

    if (!clock_pointer) {
        clock_pointer = list_entry (list_head (&frame_table), struct frame_table_entry, elem);
    }

    while (1) {
        if (pagedir_is_accessed (clock_pointer->owner->pagedir, clock_pointer->upage)) {
            pagedir_set_accessed (clock_pointer->owner->pagedir, clock_pointer->upage, false);
        }
        else {
            victim = clock_pointer;
            if (list_next (&clock_pointer->elem) == list_tail (&frame_table)) {
                clock_pointer = list_entry (list_head (&frame_table), struct frame_table_entry, elem);
            }
            else {
                clock_pointer = list_entry (list_next (&clock_pointer->elem), struct frame_table_entry, elem);
            }
            break;
        }

        if (list_next (&clock_pointer->elem) == list_tail (&frame_table)) {
            clock_pointer = list_entry (list_head (&frame_table), struct frame_table_entry, elem);
        }
        else {
            clock_pointer = list_entry (list_next (&clock_pointer->elem), struct frame_table_entry, elem);
        }
    }

    return victim;
}