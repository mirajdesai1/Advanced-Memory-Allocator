#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "xmalloc.h"

// TODO: This file should be replaced by another allocator implementation.
//
// If you have a working allocator from the previous HW, use that.
//
// If your previous homework doesn't work, you can use the provided allocator
// taken from the xv6 operating system. It's in xv6_malloc.c
//
// Either way:
//  - Replace xmalloc and xfree below with the working allocator you selected.
//  - Modify the allocator as nessiary to make it thread safe by adding exactly
//    one mutex to protect the free list. This has already been done for the
//    provided xv6 allocator.
//  - Implement the "realloc" function for this allocator.

typedef struct free_list_cell {
    size_t size;
    struct free_list_cell* next;
} free_list_cell;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
const size_t PAGE_SIZE = 4096;
static free_list_cell* free_list_head;

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void
insert(free_list_cell* block) {
    if (block->size < 16) {
        return;
    }

    if (free_list_head) {
        free_list_cell* temp = free_list_head;
        free_list_cell* prev = 0; 
        
        while (temp) {
            if ((void *) block < (void *) temp) {
                if ((void *) block + block->size == (void *) temp) {
                    block->next = temp->next;
                    block->size += temp->size;
                    if (prev) {
                        prev->next = block;
                    }
                    else {
                        free_list_head = block;
                    }
                    
                    return;
                }
                
                block->next = temp;
                if (prev) {
                    prev->next = block;
                }
                else {
                    free_list_head = block;
                }
                return;
            }
            else {
                if (((void *) temp + temp->size == (void *) block)) {
                    if ((void *) block + block->size == (void*) temp->next) {
                        temp->size += block->size + temp->next->size;
                        temp->next = temp->next->next;
                        return;
                    }

                    temp->size += block->size; 
                    return;
                }
                
                prev = temp;
                temp = temp->next;
            }
        }
    }
    else {
        free_list_head = block;
    }
}

void*
xmalloc(size_t bytes)
{
    pthread_mutex_lock(&lock);
    bytes += sizeof(size_t);
    
    free_list_cell* block = 0;

    if (bytes < PAGE_SIZE) {
        if (free_list_head) {
            free_list_cell* temp = free_list_head;
            free_list_cell* prev = 0;
            while (temp) {
                if (temp->size >= bytes) {
                    block = temp;
                    if (prev) {
                        prev->next = temp->next;
                    }
                    else {
                        free_list_head = temp->next;
                    }

                    free_list_cell* freed_block = (free_list_cell *) ((void *) block + bytes);

                    if (temp->size - bytes > 0) {
                        freed_block->size = temp->size - bytes;
                        insert(freed_block);
                    }
                    
                    block->size = bytes;
                    
                    pthread_mutex_unlock(&lock);
                    return (void *) block + sizeof(size_t);
                }
                prev = temp;
                temp = temp->next;
            }
        }
        
    }
    else {
        size_t pages = div_up(bytes, PAGE_SIZE);
        block = mmap(0, pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        block->size = pages * PAGE_SIZE;

        pthread_mutex_unlock(&lock);
        return (void *) block + sizeof(size_t);
    }
    
    block = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    block->size = bytes;
    free_list_cell* freed_block = (free_list_cell *) ((void *) block + bytes);
    freed_block->size = PAGE_SIZE - bytes;
    insert(freed_block);

    pthread_mutex_unlock(&lock);
    return (void *) block + sizeof(size_t);
}

void
xfree(void* ptr)
{
    pthread_mutex_lock(&lock);

    free_list_cell* block = (free_list_cell *) (ptr - sizeof(size_t));
    
    if (block->size < PAGE_SIZE) {
        insert(block);
    }
    else {
        munmap((void *) block, block->size);
    }

    pthread_mutex_unlock(&lock);
}

void*
xrealloc(void* prev, size_t bytes)
{
    void* temp = xmalloc(bytes);
    free_list_cell* oldblock = (free_list_cell*) (prev - sizeof(size_t));
    size_t oldsize = oldblock->size - sizeof(size_t);

    size_t minbytes = (bytes < oldsize) ? bytes : oldsize;

    memcpy(temp, prev, minbytes);

    xfree(prev);

    return temp;
}
