#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/file.h"
#include "lib/kernel/hash.h"

/* Entry of supplemental page table. */
struct supplemental_page_table_entry {
    void* upage;        /* Virtual address. */
    void* kpage;        /* Physical address. */

    int status;         /* 0: Not loaded, 1: In physical memory, 2: In swap disk */

    bool is_dirty;      /* Dirty bit. */
    bool is_accessed;   /* Access bit. */

    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    struct hash_elem elem;
};

void spt_init ();
bool insert_unmapped_spte (struct file*, off_t, void*, uint32_t, uint32_t, bool);
bool load_file_page (struct supplemental_page_table_entry*);

#endif