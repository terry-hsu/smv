/// Author: Terry Hsu
/// Test case for changing privileges

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ribbon_lib.h>
#include <memdom_lib.h>
#include <pthread.h>
#define NUM_THREADS 10
#define NUM_RIBBONS_PER_THREAD 100

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
    return NULL;
}

int main(){
    int i = 0;
    int ribbon_id[NUM_RIBBONS_PER_THREAD];
    pthread_t tid[NUM_THREADS];

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, fn, NULL);
    }
   
    // main thread create ribbons
    for (i = 0; i < NUM_RIBBONS_PER_THREAD; i++) {
        ribbon_id[i] = ribbon_create();
    }
   
    int memdom_id = memdom_create();
    int privs = 0;

    if (ribbon_is_in_domain(memdom_id, ribbon_id[0])) {
        printf("1 ribbon %d joined memdom %d\n", ribbon_id[0], memdom_id);        
    }
    ribbon_join_domain(memdom_id, ribbon_id[0]);
    if (ribbon_is_in_domain(memdom_id, ribbon_id[0])) {
        printf("2 ribbon %d joined memdom %d\n", ribbon_id[0], memdom_id);        
    }

    memdom_priv_add(memdom_id, ribbon_id[0], MEMDOM_READ);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    memdom_priv_add(memdom_id, ribbon_id[0], MEMDOM_WRITE);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    memdom_priv_add(memdom_id, ribbon_id[0], MEMDOM_EXECUTE);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    memdom_priv_add(memdom_id, ribbon_id[0], MEMDOM_ALLOCATE);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    memdom_priv_del(memdom_id, ribbon_id[0], MEMDOM_EXECUTE);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);
    
    memdom_priv_del(memdom_id, ribbon_id[0], MEMDOM_WRITE);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    memdom_priv_del(memdom_id, ribbon_id[0], MEMDOM_READ);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    memdom_priv_del(memdom_id, ribbon_id[0], MEMDOM_ALLOCATE);
    privs = memdom_priv_get(memdom_id, ribbon_id[0]);
    printf("ribbon %d privs %x memdom %d\n", ribbon_id[0], privs, memdom_id);

    ribbon_leave_domain(memdom_id, ribbon_id[0]);

    // wait for child threads
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    for (i = 0; i < NUM_RIBBONS_PER_THREAD; i++) {
        if (ribbon_id[i] != -1) {
            ribbon_kill(ribbon_id[i]);
        }
    }

    // Try delete a non-existing ribbon/memdom
    ribbon_kill(12345);
    memdom_kill(48);
    return 0;
}
