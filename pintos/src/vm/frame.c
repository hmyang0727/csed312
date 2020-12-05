#include <list.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"

static struct list frame_table;         /* Frame table. */
static struct lock frame_table_lock;    /* Lock for frame table. */
static struct lock clock_pointer_lock;

/* Frame table entry. */
struct frame_table_entry {
    void* upage;   /* Virtual address. */
    void* kpage;   /* Physical address. */
    struct thread* owner;   /* Which process is owning this frame? */
    struct list_elem elem;
};

static struct frame_table_entry* clock_pointer;
static struct list_elem* clock_ptr;

static struct frame_table_entry* get_victim (struct thread*);

void frame_init () {
    list_init (&frame_table);
    lock_init (&frame_table_lock);
    lock_init (&clock_pointer_lock);
    clock_pointer = NULL;
    clock_ptr = NULL;
}

void* alloc_frame_entry (enum palloc_flags flags, uint8_t* upage) {
    void* frame;
    struct frame_table_entry* fte, *victim;
    //struct list_elem* victim_elem;
    struct supplemental_page_table_entry* victim_spte;
    struct thread* t = thread_current ();
    size_t swap_index;

    fte = malloc(sizeof(struct frame_table_entry));

    ASSERT (fte != NULL);

PALLOC:
    frame = palloc_get_page (PAL_USER | flags);
    if(!frame) {
        /* Eviction. */
        lock_acquire (&frame_table_lock);
        victim = get_victim (t);
        
        list_remove (&victim->elem);
        
        victim_spte = find_spte (victim->upage);

        /* Page is dirty and memory-mapped file: Write back. */
        if (victim_spte->is_mmap && pagedir_is_dirty (victim->owner->pagedir, victim->upage)) {
            file_seek (victim_spte->file, victim_spte->ofs);
            file_write (victim_spte->file, victim_spte->kpage, victim_spte->read_bytes);
            victim_spte->status = 0;
            pagedir_clear_page (victim->owner->pagedir, victim->upage);
        }
        /* Page is dirty and originated from file, not mmap file: Swap. */
        else if (!victim_spte->is_mmap && pagedir_is_dirty (victim->owner->pagedir, victim->upage)) {
            swap_index = alloc_swap_slot (victim->kpage);
            victim_spte->status = 2;
            victim_spte->swap_index = swap_index;
            pagedir_clear_page (victim->owner->pagedir, victim->upage);
        }
        /* Page is not dirty and vaddr is in the stack area: Swap. */
        else if (!pagedir_is_dirty (victim->owner->pagedir, victim->upage) && PHYS_BASE - 0x800000 <= victim->upage) {
            swap_index = alloc_swap_slot (victim->kpage);
            victim_spte->status = 2;
            victim_spte->swap_index = swap_index;
            pagedir_clear_page (victim->owner->pagedir, victim->upage);
        }
        /* Ignore. */
        else {
            victim_spte->status = 0;
            pagedir_clear_page (victim->owner->pagedir, victim->upage);
        }

        victim_spte->kpage = NULL;
        victim->kpage = NULL;
        lock_release (&frame_table_lock);
        
        goto PALLOC;
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
            lock_acquire(&clock_pointer_lock);
            if (clock_pointer == target_fte) {
                clock_pointer = NULL;
            }
            lock_release(&clock_pointer_lock);
            list_remove (e);
            free (target_fte);
            break;
        }
    }
    lock_release (&frame_table_lock);

    palloc_free_page (kpage);
}

/* Select victim based on clock algorithm. */
static struct frame_table_entry* get_victim (struct thread* t) {
    struct list_elem* victim;

    lock_acquire(&clock_pointer_lock);

    if(!clock_ptr) { clock_ptr = list_begin(&frame_table); }

    while(1) {
        clock_pointer = list_entry (clock_ptr, struct frame_table_entry, elem);

        if (pagedir_is_accessed (t->pagedir, clock_pointer->upage)) {
            pagedir_set_accessed (t->pagedir, clock_pointer->upage, false);
            clock_ptr = list_next(clock_ptr) == list_end(&frame_table) ? list_begin(&frame_table) : list_next(clock_ptr);
        }

        else {
            victim = clock_pointer;
            clock_ptr = list_next(clock_ptr) == list_end(&frame_table) ? list_begin(&frame_table) : list_next(clock_ptr);
            break;
        }
    }

    lock_release(&clock_pointer_lock);

    // if (!clock_pointer) {
    //     clock_pointer = list_entry (list_begin (&frame_table), struct frame_table_entry, elem);
    // }

    // while (1) {
    //     printf("CLOCK POINTER -> OWNER -> PAGEDIR: %p\n", clock_pointer->owner->pagedir);
    //     if (pagedir_is_accessed (clock_pointer->owner->pagedir, clock_pointer->upage)) {
    //         pagedir_set_accessed (clock_pointer->owner->pagedir, clock_pointer->upage, false);
    //     }
    //     else {
    //         victim = clock_pointer;
    //         if (list_next (&clock_pointer->elem) == list_end (&frame_table)) {
    //             clock_pointer = list_entry (list_begin (&frame_table), struct frame_table_entry, elem);
    //         }
    //         else {
    //             clock_pointer = list_entry (list_next (&clock_pointer->elem), struct frame_table_entry, elem);
    //         }
    //         break;
    //     }

    //     if (list_next (&clock_pointer->elem) == list_end (&frame_table)) {
    //         clock_pointer = list_entry (list_begin (&frame_table), struct frame_table_entry, elem);
    //     }
    //     else {
    //         clock_pointer = list_entry (list_next (&clock_pointer->elem), struct frame_table_entry, elem);
    //     }
    // }
    return victim;
}