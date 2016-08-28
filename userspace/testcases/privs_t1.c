/// Author: Terry Hsu
/// Test case for changing privileges

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <smv_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 10
#define NUM_SMVS_PER_THREAD 10

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
    return NULL;
}

int main(){
    int i = 0;
    int smv_id[NUM_SMVS_PER_THREAD];
    pthread_t tid[NUM_THREADS];

    int memdom_id = memdom_create();
    int privs = 0;

    // main thread create smvs
    for (i = 0; i < NUM_SMVS_PER_THREAD; i++) {
        smv_id[i] = smv_create();
    }

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, fn, NULL);
    }
   
    smv_join_domain(memdom_id, smv_id[0]);
    if (smv_is_in_domain(memdom_id, smv_id[0])) {
        printf("smv %d joined memdom %d\n", smv_id[0], memdom_id);        
    }

    /* Add privilege and delete them in serve order */
    memdom_priv_add(memdom_id, smv_id[0], MEMDOM_READ);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    memdom_priv_add(memdom_id, smv_id[0], MEMDOM_WRITE);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    memdom_priv_add(memdom_id, smv_id[0], MEMDOM_EXECUTE);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    memdom_priv_add(memdom_id, smv_id[0], MEMDOM_ALLOCATE);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    memdom_priv_del(memdom_id, smv_id[0], MEMDOM_EXECUTE);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);
    
    memdom_priv_del(memdom_id, smv_id[0], MEMDOM_WRITE);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    memdom_priv_del(memdom_id, smv_id[0], MEMDOM_READ);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    memdom_priv_del(memdom_id, smv_id[0], MEMDOM_ALLOCATE);
    privs = memdom_priv_get(memdom_id, smv_id[0]);
    printf("smv %d privs %x memdom %d\n", smv_id[0], privs, memdom_id);

    smv_leave_domain(memdom_id, smv_id[0]);

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    for (i = 0; i < NUM_SMVS_PER_THREAD; i++) {
        if (smv_id[i] != -1) {
            smv_kill(smv_id[i]);
        }
    }

    // Try delete a non-existing smv/memdom
    smv_kill(12345);
    memdom_kill(48);
    return 0;
}
