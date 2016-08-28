/// Author: Terry Hsu
/// Test case for creating ribbon threads
/// Each thread running in its own ribbon

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ribbon_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 10

// Thread stack for creating ribbons
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
    int ribbon_id[NUM_THREADS];
    pthread_t tid[NUM_THREADS];

    ribbon_main_init(0);

    int memdom_id = memdom_create();
    int privs = 0;

    // main thread create ribbons
    for (i = 0; i < NUM_THREADS; i++) {
        ribbon_id[i] = ribbon_create();
        ribbon_join_domain(0, ribbon_id[i]);
        memdom_priv_add(0, ribbon_id[i], MEMDOM_READ | MEMDOM_WRITE);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        rv = smvthread_create(ribbon_id[i], &tid[i], fn, NULL);
        if (rv == -1) {
            printf("smvthread_create error\n");
        }
    }

    /* Add privilege and delete them in serve order */
    memdom_priv_add(memdom_id, ribbon_id[0], MEMDOM_READ);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    ribbon_leave_domain(memdom_id, ribbon_id[0]);

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
    return 0;
}
