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

/* Memory domain metadata structure
 * A memory domain is an anonymously mmap-ed memory area.
 * mmap() is called when memdom_alloc is called the first time for a given memdom 
 * Subsequent allocation does not invoke mmap(), instead, it allocates memory from the mmaped
 * area and update related metadata fields. 
 */
struct memdom_metadata_struct {
    int memdom_id;
    unsigned long start;    // start of this memdom's addr (inclusive)
    unsigned long end;      // end of this memdom's addr (exclusive) 
    unsigned long current;  // the current available memory address for allocation
    unsigned long remaining;  // the memory size of unallocated area at the end of this memdom (holes in the middle are not considered)
    unsigned long total_size; // the total memory size of this memdom
    pthread_mutex_t mlock;  // protects this memdom in sn SMP environment
};

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
void memdom_free(int memdom_id, void* data);

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
