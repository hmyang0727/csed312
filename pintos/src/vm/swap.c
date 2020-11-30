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

static struct block *swap_block;
static struct bitmap *swap_available;
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE; // 8 sectors are needed to store 1 page.
static size_t swap_size;


void swap_init() {
    swap_block = block_get_role(BLOCK_SWAP);

    ASSERT(swap_block == NULL);

    swap_size = block_size(swap_block) / SECTORS_PER_PAGE; // swap_size = (size of swap disk) / (# of pages to store one page) 
    swap_available = bitmap_create(swap_size); 
    bitmap_set_all(swap_available, true); // set all true, all sectors in the swap disk are free now.
    return;
}
