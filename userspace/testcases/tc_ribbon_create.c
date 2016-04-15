/// Author: Terry Hsu
/// Test case for creating ribbons

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <ribbon_lib.h>
#include <memdom_lib.h>
#define NUM_THREADS 10
#define NUM_RIBBONS_PER_THREAD 5

// Thread stack for creating ribbons
void *fn(void *args){
    int i = 0;
    int ribbon_id[NUM_RIBBONS_PER_THREAD];
    for (i = 0; i < NUM_RIBBONS_PER_THREAD; i++) {
        ribbon_id[i] = ribbon_create();
    }
    for (i = 0; i < NUM_RIBBONS_PER_THREAD; i++) {
        if (ribbon_id[i] != -1) {
            ribbon_kill(ribbon_id[i]);
        }
    }
    printf("Hi i is in %d!\n", memdom_query_id(&i));
    return NULL;
}

int main(){
    int i = 0;
    int ribbon_id[NUM_RIBBONS_PER_THREAD];
    pthread_t tid[NUM_THREADS];

    ribbon_main_init(1);

    for (i = 0; i < NUM_THREADS; i++) {
        fprintf(stderr, "creating %d thread\n", i);
        pthread_create(&tid[i], NULL, fn, NULL);
    }

    // main thread create ribbons
    for (i = 0; i < NUM_RIBBONS_PER_THREAD; i++) {
        ribbon_id[i] = ribbon_create();
    }
    for (i = 0; i < NUM_RIBBONS_PER_THREAD; i++) {
        if (ribbon_id[i] != -1) {
            ribbon_kill(ribbon_id[i]);
        }
    }

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    // Try delete a non-existing ribbon/memdom
    ribbon_kill(12345);
    memdom_kill(1025);

    printf("main memdom id: %d\n", memdom_main_id());
    return 0;
}
