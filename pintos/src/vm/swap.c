#include <bitmap.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap_block;
static struct bitmap *swap_available;
static size_t swap_size;
static struct lock swap_lock;

void swap_init () {
    swap_block = block_get_role (BLOCK_SWAP);

    ASSERT (swap_block != NULL);

    swap_size = block_size (swap_block) / SECTORS_PER_PAGE; // swap_size = (size of swap disk) / (# of pages to store one page)
    swap_available = bitmap_create (swap_size);
    bitmap_set_all (swap_available, true); // set all true, all sectors in the swap disk are free now.
    lock_init (&swap_lock);
    return;
}

size_t alloc_swap_slot (void* kpage) {
    size_t swap_index;
    int position;

    lock_acquire (&swap_lock);

    swap_index = bitmap_scan_and_flip (swap_available, 0, 1, true);

    ASSERT (swap_index != BITMAP_ERROR);

    for (position = 0; position < SECTORS_PER_PAGE; position++) {
        block_write (swap_block, swap_index * SECTORS_PER_PAGE + position, kpage + position * BLOCK_SECTOR_SIZE);
    }

    lock_release (&swap_lock);

    return swap_index;
}

void free_swap_slot (size_t swap_index, void* kpage) {
    int position;

    lock_acquire (&swap_lock);

    for (position = 0; position < SECTORS_PER_PAGE; position++) {
        block_read (swap_block, swap_index * SECTORS_PER_PAGE + position, kpage + position * BLOCK_SECTOR_SIZE);
    }

    bitmap_set (swap_available, swap_index, true);

    lock_release (&swap_lock);
}

void destroy_swap_slot (size_t swap_index) {
    bitmap_set (swap_available, swap_index, true);
}