#ifndef MEMDOM_LIB_H
#define MEMDOM_LIB_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "ribbon_lib.h"
#include "kernel_comm.h"

/* Permission */
#define MEMDOM_READ             0x00000001
#define MEMDOM_WRITE            0x00000002
#define MEMDOM_EXECUTE          0x00000004
#define MEMDOM_ALLOCATE         0x00000008

#ifdef __cplusplus
extern "C" {
#endif

/* Create memory domain and return it to user */
int memdom_create(void);

/* Remove memory domain memdom from kernel */
int memdom_kill(int memdom_id);

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
