/// Author: Terry Hsu
/// Test case for creating smvs

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <smv_lib.h>
#include <memdom_lib.h>
#define NUM_THREADS 10
#define NUM_SMVS_PER_THREAD 5

// Thread stack for creating smvs
void *fn(void *args){
    int i = 0;
    int smv_id[NUM_SMVS_PER_THREAD];
    for (i = 0; i < NUM_SMVS_PER_THREAD; i++) {
        smv_id[i] = smv_create();
    }
    for (i = 0; i < NUM_SMVS_PER_THREAD; i++) {
        if (smv_id[i] != -1) {
            smv_kill(smv_id[i]);
        }
    }
    printf("Hi i is in %d!\n", memdom_query_id(&i));
    return NULL;
}

int main(){
    int i = 0;
    int smv_id[NUM_SMVS_PER_THREAD];
    pthread_t tid[NUM_THREADS];

    smv_main_init(1);

    for (i = 0; i < NUM_THREADS; i++) {
        fprintf(stderr, "creating %d thread\n", i);
        pthread_create(&tid[i], NULL, fn, NULL);
    }

    // main thread create smvs
    for (i = 0; i < NUM_SMVS_PER_THREAD; i++) {
        smv_id[i] = smv_create();
    }
    for (i = 0; i < NUM_SMVS_PER_THREAD; i++) {
        if (smv_id[i] != -1) {
            smv_kill(smv_id[i]);
        }
    }

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    // Try delete a non-existing smv/memdom
    smv_kill(12345);
    memdom_kill(1025);

    printf("main memdom id: %d\n", memdom_main_id());
    return 0;
}
