# optimized-memory-allocator
CS3650 Computer Systems challenge assignment 2

Optimized bucket memory allocator 

In the “bucket” allocator, we shoot for an O(1) allocation performance by completely separating our free lists. Each free list is placed in its appropriate bucket. When an allocation request of a given size comes in, we allocate a new block of memory specifically to put on that size-specific free list (or “bucket”).

### opt_malloc.c
Optimized bucket memory allocator
