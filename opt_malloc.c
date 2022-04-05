#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include "xmalloc.h"

typedef struct free_list_cell {
    size_t size;
    struct free_list_cell* next;
} free_list_cell;

const size_t PAGE_SIZE = 4096;
const size_t MAX_BIN_SIZE = 2048;
//static free_list_cell* free_list_head;

__thread free_list_cell* free_bins[8];
//const size_t free_bins_sizes[] = {16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048};
const size_t free_bins_sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};

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

int
getindex(size_t size) {
    return (int) (ceil(log(size) / log(2))) - 4;
    
    /*
    int prev = log(size) / log(2);
    int next = prev + 1;
    int prevsize = free_bins_sizes[2 * (prev - 4)];
    int nextsize = free_bins_sizes[2 * (next - 4)];
    int midlog = log((prevsize + nextsize) / 2) / log(2);

    int val = log(size) / log(2);
    if (val <= prev) {
        return 2 * (prev - 4);
    }
    else if ()



    return (int) 2 * (log(size) / log(2)) - 4
    */
}

void
insert(free_list_cell* block) {
    int index = getindex(block->size);

    free_list_cell* temp = free_bins[index];
    free_bins[index] = block;
    block->next = temp;
}

void*
xmalloc(size_t bytes)
{
    bytes += sizeof(size_t);
    
    free_list_cell* block = 0;

    if (bytes <= MAX_BIN_SIZE) {
        if (bytes < sizeof(free_list_cell)) {
            bytes = sizeof(free_list_cell);
        }

        //printf("%ld : ", bytes);
        int index = getindex(bytes);
        //printf("%d\n", index);

        if (free_bins[index]) {
            block = free_bins[index];
            //block->size = free_bins_sizes[index];
            free_bins[index] = block->next;
        }
        else {
            //printf("im here\n");
            int sizeofbin = free_bins_sizes[index];
            int num_bins = PAGE_SIZE / sizeofbin;
            void* temp = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            free_list_cell* prev = (free_list_cell *) temp;
            prev->size = sizeofbin;
            free_list_cell* next = 0;
            for (int i = 1; i < num_bins; i++) {
                next = (free_list_cell *) (temp + i * sizeofbin);
                next->size = sizeofbin;
                prev->next = next;
                prev = next;
            }
            prev->next = 0;
            //printf("%d\n", i);
            free_bins[index] = (free_list_cell *) temp;
            
            block = free_bins[index];
            //block->size = sizeofbin;
            free_bins[index] = block->next;
        }

        return (void *) block + sizeof(size_t);
    }
    else {
        //printf("im also here\n");
        size_t pages = div_up(bytes, PAGE_SIZE);
        block = mmap(0, pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        block->size = pages * PAGE_SIZE;

        return (void *) block + sizeof(size_t);
    }    
}

void
xfree(void* ptr)
{

    free_list_cell* block = (free_list_cell *) (ptr - sizeof(size_t));
    
    if (block->size <= MAX_BIN_SIZE) {
        insert(block);
    }
    else {
        munmap((void *) block, block->size);
    }
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

