#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"

unsigned spt_hash_hash_func (const struct hash_elem* e, void* aux) {
    struct supplemental_page_table_entry* spte;

    spte = hash_entry (e, struct supplemental_page_table_entry, elem);

    return hash_int (spte->upage);
}

bool spt_hash_less_func (const struct hash_elem* a, const struct hash_elem* b, void* aux) {
    struct supplemental_page_table_entry* spte_a, *spte_b;

    spte_a = hash_entry (a, struct supplemental_page_table_entry, elem);
    spte_b = hash_entry (b, struct supplemental_page_table_entry, elem);

    return spte_a->upage < spte_b->upage ? true : false;
}

void spt_init (struct hash* supplemental_page_table, struct lock* supplemental_page_table_lock) {
    hash_init (supplemental_page_table, &spt_hash_hash_func, &spt_hash_less_func, NULL);
    lock_init (supplemental_page_table_lock);
}

bool insert_unmapped_spte (struct file* file, off_t ofs, void* upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    struct supplemental_page_table_entry* spte;

    spte = malloc(sizeof(struct supplemental_page_table_entry));

    ASSERT (spte != NULL);

    spte->status = 0; /* Status: Not loaded. */
    spte->file = file;
    spte->ofs = ofs;
    spte->upage = upage;
    spte->kpage = NULL;
    spte->read_bytes = read_bytes;
    spte->zero_bytes = zero_bytes;
    spte->writable = writable;
    spte->is_dirty = false;
    spte->is_accessed = false;

    if(!hash_insert (&thread_current ()->supplemental_page_table, &spte->elem)) {
        return true;
    }
    else {
        free (spte);
        return false;
    }
}

bool load_file_page (struct supplemental_page_table_entry* spte) {
    uint8_t *kpage;

    file_seek(spte->file, spte->ofs);

    /* Get a page of memory. */
    kpage = alloc_frame_entry(PAL_USER, spte->upage);
    if (kpage == NULL)
        return false;

    /* Load this page. */
    if (file_read(spte->file, kpage, spte->read_bytes) != (int)spte->read_bytes)
    {
        free_frame_entry (kpage);
        return false;
    }
    memset(kpage + spte->read_bytes, 0, spte->zero_bytes);

    /* Add the page to the process's address space. */
    if (!(pagedir_get_page(thread_current ()->pagedir, spte->upage) == NULL 
          && pagedir_set_page(thread_current ()->pagedir, spte->upage, kpage, spte->writable)))
    {
        free_frame_entry (kpage);
        return false;
    }

    spte->status = 1; /* Status: In physical memory. */
    return true;
}