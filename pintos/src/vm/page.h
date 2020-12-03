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

    bool is_mmap;

    struct hash_elem elem;
};

/* Initialize supplemental page table and its lock. */
void spt_init (struct hash*, struct lock*);

/* Insert supplemental page table entry. */
bool insert_unmapped_spte (struct file* file, off_t ofs, void* upage, void* kpage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int status, bool is_mmap);

/* Load file page that has not been loaded. */
bool load_file_page (struct supplemental_page_table_entry*);

/* Grow stack. */
void grow_stack (void* fault_addr);

/* Find supplemental page table entry using virtual address. */
struct supplemental_page_table_entry* find_spte (void*);

#endif