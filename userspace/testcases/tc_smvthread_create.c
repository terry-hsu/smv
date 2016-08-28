/// Author: Terry Hsu
/// Test case for creating smv threads
/// Each thread running in its own smv

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <smv_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 10

// Thread stack for creating smvs
// Each thread write to 1 page
void *fn(void *args){
    int i = 0;
    int j[1024];
    for (i = 0; i < 1024; i++) {
        j[i] = i;
        if (i % 1000 == 0) {
            printf("j[%d] = %d\n", i, j[i]);
        }
    }
    return NULL;
}

int main(){
    int i = 0;
    int rv = 0;
    int smv_id[NUM_THREADS];
    pthread_t tid[NUM_THREADS];

    smv_main_init(0);

    int memdom_id = memdom_create();
    int privs = 0;

    // main thread create smvs
    for (i = 0; i < NUM_THREADS; i++) {
        smv_id[i] = smv_create();
        smv_join_domain(0, smv_id[i]);
        memdom_priv_add(0, smv_id[i], MEMDOM_READ | MEMDOM_WRITE);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        rv = smvthread_create(smv_id[i], &tid[i], fn, NULL);
        if (rv == -1) {
            printf("smvthread_create error\n");
        }
    }

    /* Add privilege and delete them in serve order */
    memdom_priv_add(memdom_id, smv_id[0], MEMDOM_READ);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    smv_leave_domain(memdom_id, smv_id[0]);

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
        printf("waited thread %d\n", i);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        if (smv_id[i] != -1) {
            smv_kill(smv_id[i]);
        }
    }
    return 0;
}
