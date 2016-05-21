#ifndef RIBBON_LIB_H
#define RIBBON_LIB_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include <string.h>
#include <pthread.h>
#include "memdom_lib.h"
#include "kernel_comm.h"

#define LOGGING 0
#define __SOURCEFILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define rlog(format, ...) { \
    if( LOGGING ) { \
        fprintf(stdout, "[smv] " format, ##__VA_ARGS__); \
        fflush(NULL);   \
    }\
}

//#define THREAD_PRIVATE_STACK
#define NEW_RIBBON -1

#define INTERCEPT_PTHREAD_CREATE
#ifdef INTERCEPT_PTHREAD_CREATE
#define pthread_create(tid, attr, fn, args) ribbon_thread_create(NEW_RIBBON, tid, fn, args)
#endif

extern int ALLOW_GLOBAL; // 1: all threads can access global memdom, 0 otherwise

#ifdef __cplusplus
extern "C" {
#endif

/* Telling the kernel that this process will be using the secure memory view model */
int ribbon_main_init(int);

/* Create a ribbon and return the ID of the newly created ribbon */
int ribbon_create(void);

/* Destroy a ribbon */
int ribbon_kill(int ribbon_id);

/* Add ribbon to memory domain */
int ribbon_join_domain(int memdom_id, int ribbon_id);

/* Remove ribbon from memory domain */
int ribbon_leave_domain(int memdom_id, int ribbon_id);

/* Check if ribbon is in memory domain, return 1 if true, 0 otherwise */
int ribbon_is_in_domain(int memdom_id, int ribbon_id);

/* Create an smv thread running in a ribbon */
int ribbon_thread_create(int ribbon_id, pthread_t *tid, void *(fn)(void*), void *args);

/* Check whether a ribbon exists */
int ribbon_exists(int ribbon_id);
#ifdef __cplusplus
}
#endif

#endif
