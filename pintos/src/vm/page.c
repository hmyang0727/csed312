#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "lib/kernel/hash.h"

// struct hash supplemental_page_table;
// struct lock supplemental_page_table_lock;

struct supplemental_page_table_entry {
    void* upage;        /* Virtual address. */
    void* kpage;        /* Physical address. */

    int status;         /* 1: In physical memory, 2: In swap disk */

    bool is_dirty;      /* Dirty bit. */
    bool is_accessed;   /* Access bit. */

    struct hash_elem elem;
};

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

