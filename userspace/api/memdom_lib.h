#ifndef MEMDOM_LIB_H
#define MEMDOM_LIB_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "ribbon_lib.h"
#include "kernel_comm.h"

/* Permission */
#define MEMDOM_READ             0x00000001
#define MEMDOM_WRITE            0x00000002
#define MEMDOM_EXECUTE          0x00000004
#define MEMDOM_ALLOCATE         0x00000008

/* MMAP flag for memdom protected area */
#define MAP_MEMDOM	0x00800000	

/* Maximum heap size a memdom can use: 1GB */
//#define MEMDOM_HEAP_SIZE 0x40000000
#define MEMDOM_HEAP_SIZE 0x1000

/* Maximum number of memdoms a thread can have: 1024*/
#define MAX_MEMDOM 1024

/* Minimum size of bytes to allocate in one chunk */
#define CHUNK_SIZE 64

/* Free list structure
 * A free list struct records a block of memory available for allocation.
 * memdom_alloc() allocates memory from the tail of the free list (usually the largest available block).
 * memdom_free() inserts free list to the head of the free list
 */
struct free_list_struct {
    void *addr;
    unsigned long size;
    struct free_list_struct *next;
};

/* Every allocated chunk of memory has this block header to record the required
 * metadata for the allocator to free memory
 */
struct block_header_struct {
    void *addr;
    int memdom_id;
    unsigned long size;    
};

/* Memory domain metadata structure
 * A memory domain is an anonymously mmap-ed memory area.
 * mmap() is called when memdom_alloc is called the first time for a given memdom 
 * Subsequent allocation does not invoke mmap(), instead, it allocates memory from the mmaped
 * area and update related metadata fields. 
 */
struct memdom_metadata_struct {
    int memdom_id;
    void *start;    // start of this memdom's addr (inclusive)
    unsigned long total_size; // the total memory size of this memdom
    struct free_list_struct *free_list_head;
    struct free_list_struct *free_list_tail;
    pthread_mutex_t mlock;  // protects this memdom in sn SMP environment
};
struct memdom_metadata_struct *memdom[MAX_MEMDOM];

#ifdef __cplusplus
extern "C" {
#endif

/* Create memory domain and return it to user */
int memdom_create(void);

/* Remove memory domain memdom from kernel */
int memdom_kill(int memdom_id);

/* Allocate memory region in memory domain memdom */
void *memdom_mmap(int memdom_id, 
                  unsigned long addr, unsigned long len, 
                  unsigned long prot, unsigned long flags, 
                  unsigned long fd, unsigned long pgoff);

/* Allocate npages pages in memory domain memdom */
void *memdom_alloc(int memdom_id, unsigned long nbytes);

/* Deallocate npages pages in memory domain memdom */
void memdom_free(void* data);

/* Return privilege status of ribbon rib in memory domain memdom */
unsigned long memdom_priv_get(int memdom_id, int ribbon_id);

/* Add privilege of ribbon rib in memory domain memdom */
int memdom_priv_add(int memdom_id, int ribbon_id, unsigned long privs);

/* Delete privilege of ribbon rib in memory domain memdom */
int memdom_priv_del(int memdom_id, int ribbon_id, unsigned long privs);

/* Modify privilege of ribbon rib in memory domain memdom */
int memdom_priv_mod(int memdom_id, int ribbon_id, unsigned long privs);

/* Get the memdom id for global memory used by main thread */
int memdom_main_id(void);

#ifdef __cplusplus
}
#endif

#endif
