/// Author: Terry Hsu
/// Test case for mutex locking/unlocking

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <ribbon_lib.h>
#include <memdom_lib.h>
int global_int;
pthread_mutex_t lock;
pthread_cond_t cond;

// Thread stack for creating ribbons
void *fn_1(void *args){
    pthread_mutex_lock(&lock);
    while(global_int != 999)
        pthread_cond_wait(&cond, &lock);
    printf("fn1: read global_int: %d\n", global_int);
    global_int++;
    printf("fn1: updated global_int: %d\n", global_int);
    pthread_mutex_unlock(&lock);

    return NULL;
}
void *fn_2(void *args){
    sleep(3);
    pthread_mutex_lock(&lock);
    global_int = 999;
    printf("fn2: signal\n");
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(){
    int ribbon_id;
    pthread_t tid[2];
    global_int = 0;
    ribbon_main_init(1);

    /* Mutex init */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&lock, &attr);

    /* Cond init */
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&cond, &cattr);

    pthread_create(&tid[0], NULL, fn_2, NULL);
    
    ribbon_id = ribbon_create();
    ribbon_thread_create(ribbon_id, &tid[1], fn_1, NULL);

    // wait for child threads
    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    printf("global_int: %d\n", global_int);
    return 0;
}
