#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init ();
size_t alloc_swap_slot (void* kpage);
void free_swap_slot (size_t swap_index, void* kpage);
void destroy_swap_slot (size_t swap_index);

#endif