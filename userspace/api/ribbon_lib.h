#ifndef SMV_LIB_H
#define SMV_LIB_H

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
#define NEW_SMV -1

#define INTERCEPT_PTHREAD_CREATE
#ifdef INTERCEPT_PTHREAD_CREATE
#define pthread_create(tid, attr, fn, args) smvthread_create(NEW_SMV, tid, fn, args)
#endif

extern int ALLOW_GLOBAL; // 1: all threads can access global memdom, 0 otherwise

#ifdef __cplusplus
extern "C" {
#endif

/* Telling the kernel that this process will be using the secure memory view model */
int smv_main_init(int);

/* Create a smv and return the ID of the newly created smv */
int smv_create(void);

/* Destroy a smv */
int smv_kill(int smv_id);

/* Add smv to memory domain */
int smv_join_domain(int memdom_id, int smv_id);

/* Remove smv from memory domain */
int smv_leave_domain(int memdom_id, int smv_id);

/* Check if smv is in memory domain, return 1 if true, 0 otherwise */
int smv_is_in_domain(int memdom_id, int smv_id);

/* Create an smv thread running in a smv */
int smvthread_create(int smv_id, pthread_t *tid, void *(fn)(void*), void *args);

/* Check whether a smv exists */
int smv_exists(int smv_id);
#ifdef __cplusplus
}
#endif

#endif
