/// Author: Terry Hsu
/// Test case for creating memdom domains

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <smv_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 10
#define NUM_MEMDOMS_PER_THREAD 100

// Thread stack for creating memdoms
void *fn(void *args){
    int i = 0;
    int memdom_id[NUM_MEMDOMS_PER_THREAD];
    for (i = 0; i < NUM_MEMDOMS_PER_THREAD; i++) {
        memdom_id[i] = memdom_create();
    }
    for (i = 0; i < NUM_MEMDOMS_PER_THREAD; i++) {
        if (memdom_id[i] != -1) {
            memdom_kill(memdom_id[i]);
        }
    }
    return NULL;
}

int main(){
    int i = 0;
    int memdom_id[NUM_MEMDOMS_PER_THREAD];
    pthread_t tid[NUM_THREADS];

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, fn, NULL);
    }

    // main thread create memdoms
    for (i = 0; i < NUM_MEMDOMS_PER_THREAD; i++) {
        memdom_id[i] = memdom_create();
    }
    for (i = 0; i < NUM_MEMDOMS_PER_THREAD; i++) {
        if (memdom_id[i] != -1) {
//          memdom_kill(memdom_id[i]);
        }
    }

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    // Try delete a non-existing memdom
    memdom_kill(12345);
    return 0;
}
