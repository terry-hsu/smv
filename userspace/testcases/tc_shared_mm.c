/// Author: Terry Hsu
/// Test case for ribbon threads to access shared memory areas (heaps and globals)
/// Each thread running in its own ribbon

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ribbon_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 10

int global_int;
int *dint;

pthread_mutex_t mlock;

// Thread stack for creating ribbons
// Each thread write to 1 page
void *fn(void *args){
    pthread_mutex_lock(&mlock);
    global_int++;
    printf("global_int: %d\n", global_int);

    *dint = global_int + 1;
    printf("*dint: %d\n", *dint);
    pthread_mutex_unlock(&mlock);
    return NULL;
}

int main(){
    int i = 0;
    int rv = 0;
    int ribbon_id[NUM_THREADS];
    pthread_t tid[NUM_THREADS];
    global_int = 0;

    ribbon_main_init();
    
    pthread_mutex_init(&mlock, NULL);

    int memdom_id = memdom_create();
    dint = (int*)malloc(sizeof(int));
    *dint = 123;

    // main thread create ribbons
    for (i = 0; i < NUM_THREADS; i++) {
        ribbon_id[i] = ribbon_create();
        // Join local memdom
        ribbon_join_domain(memdom_id, ribbon_id[i]);    
        memdom_priv_add(memdom_id, ribbon_id[i], MEMDOM_READ | MEMDOM_WRITE);
        // Join global memdom
        ribbon_join_domain(0, ribbon_id[i]);
        memdom_priv_add(0, ribbon_id[i], MEMDOM_READ | MEMDOM_WRITE);
    }


    for (i = 0; i < NUM_THREADS; i++) {
        rv = ribbon_thread_create(ribbon_id[i], &tid[i], fn, NULL);
        if (rv) {
            printf("ribbon_thread_create error\n");
        }
    }

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
        printf("waited thread %d\n", i);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        if (ribbon_id[i] != -1) {
            ribbon_kill(ribbon_id[i]);
        }
    }

    global_int++;
    *dint = global_int + 1 ;
    printf("Final global_int: %d\n", global_int);
    printf("Final dint: %d\n", *dint);

    return 0;
}