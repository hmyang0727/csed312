#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"

void frame_init ();
void* alloc_frame_entry (enum palloc_flags, uint8_t*);
void free_frame_entry (void*);
void destory_frame_entry (struct thread* t);

#endif