/// Author: Terry Hsu
/// Test case for smv threads to allocate memory domain protected regions heaps
/// Each thread running in its own smv

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <smv_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 1

int global_int;
int *dint;

int *heap_int[NUM_THREADS];
int block_write;
pthread_mutex_t mlock;

// Thread stack for creating smvs
// Each thread write to 1 page
void *fn(void *args){
    int *t = (int*)args;
    fprintf(stderr, "smv %d read global_int: %d\n", *t, global_int);
    pthread_mutex_lock(&mlock);
    global_int++;   
    printf("smv %d wrote global_int: %d\n", *t, global_int);

    printf("smv %d read *dint: %d\n", *t, *dint);
    *dint = global_int + 1;   
    printf("smv %d wrote *dint: %d\n", *t, *dint);

    pthread_mutex_unlock(&mlock);
    return NULL;
}

int main(){
    int i = 0;
    int *memdom_int[1024];

    smv_main_init(1);
    
    pthread_mutex_init(&mlock, NULL);

    int memdom_id = memdom_create();
    smv_join_domain(memdom_id, 0);
    memdom_priv_add(memdom_id, 0, MEMDOM_READ | MEMDOM_WRITE);

    i = 0;
    memdom_int[i] = (int*) memdom_alloc(memdom_id, 0x50);
    *memdom_int[i] = i+1;
    printf("memdom_int[%d]: %d\n", i, *memdom_int[i]);
    memdom_free(memdom_int[i]);
    printf("\n");
 
    memdom_int[i] = (int*) memdom_alloc(memdom_id, 0x20);
    *memdom_int[i] = i+1;
    printf("memdom_int[%d]: %d\n", i, *memdom_int[i]);
    memdom_free(memdom_int[i]);
    printf("\n");
 

    memdom_int[i] = (int*) memdom_alloc(memdom_id, 0x50);
    *memdom_int[i] = i+1;
    printf("memdom_int[%d]: %d\n", i, *memdom_int[i]);
    memdom_free(memdom_int[i]);
    printf("\n");

    memdom_kill(memdom_id);
    return 0;
}
