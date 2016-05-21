#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <limits.h>
#include "ribbon_lib.h"

pthread_mutex_t create_thread_mutex;
int ALLOW_GLOBAL; // 1: all threads can access global memdom, 0 otherwise

/* Telling the kernel that this process will be using the secure memory view model 
 * The master thread must call this routine to notify the kernel its status */
int ribbon_main_init(int global){
    int rv = -1;
	ALLOW_GLOBAL = 0;

	/* Set mm->using_smv to true in kernel space */
	rv = message_to_kernel("ribbon,maininit");
	if (rv != 0) {
		fprintf(stderr, "ribbon_main_init() failed\n");
		return -1;
	}
    rlog("kernel responded %d\n", rv);

	/* Initialize mutex for protecting smv_thread_create */
	pthread_mutex_init(&create_thread_mutex, NULL);

	/* TODO?: create a memdom for the master thread so that memdom_alloc in domain 0 can work */

	/* Decide whether we allow all threads to access global memdom */
	ALLOW_GLOBAL = global;
	return rv;
}

/* Create a ribbon and return the ID of the new ribbon */
int ribbon_create(void) {
    int ribbon_id = -1;
	ribbon_id = message_to_kernel("ribbon,create");
	if (ribbon_id < 0) {
		fprintf(stderr, "ribbon_create() failed\n");
		return -1;
	}
    rlog("kernel responded ribbon id %d\n", ribbon_id);
	return ribbon_id;
}

/* Destroy the ribbon ribbon_id */
int ribbon_kill(int ribbon_id) {
	int rv = 0;
	char buf[100];
	sprintf(buf, "ribbon,kill,%d", ribbon_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "ribbon_kill(%d) failed\n", ribbon_id);
		return -1;
	}
	rlog("Ribbon ID %d killed", ribbon_id);
	return rv;
}

/* Add ribbon to memory domain */
int ribbon_join_domain(int memdom_id, int ribbon_id) {
	int rv = 0;
	char buf[50];
	sprintf(buf, "ribbon,domain,%d,join,%d", ribbon_id, memdom_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "ribbon_join_domain(ribbon %d, memdom %d) failed\n", ribbon_id, memdom_id);
		return -1;
	}
	rlog("Ribbon ID %d joined memdom ID %d", ribbon_id, memdom_id);
	return 0;
}

/* Remove ribbon ribbon_id from memory domain memdom */
int ribbon_leave_domain(int memdom_id, int ribbon_id) {
	int rv = 0;
	char buf[100];
	sprintf(buf, "ribbon,domain,%d,leave,%d", ribbon_id, memdom_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "ribbon_leave_domain(ribbon %d, memdom %d) failed\n", ribbon_id, memdom_id);
		return -1;
	}
	rlog("Ribbon ID %d left memdom ID %d", ribbon_id, memdom_id);
    return rv;
}

/* Check if ribbon is in memory domain */
int ribbon_is_in_domain(int memdom_id, int ribbon_id) {
	int rv = 0;
	char buf[50];
	sprintf(buf, "ribbon,domain,%d,isin,%d", ribbon_id, memdom_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "ribbon_is_in_domain(ribbon %d, memdom %d) failed\n", ribbon_id, memdom_id);
		return -1;
	}
	rlog("Ribbon ID %d in memdom ID %d?: %d", ribbon_id, memdom_id, rv);
	return rv;
}

/* Check if ribbon is in memory domain */
int ribbon_exists(int ribbon_id) {
	int rv = 0;
	char buf[50];
	sprintf(buf, "ribbon,exists,%d", ribbon_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "ribbon_exists(ribbon %d) failed\n", ribbon_id);
		return -1;
	}
	rlog("Ribbon ID %d exists? %d", ribbon_id, rv);
	return rv;
}

/* Create an smv thread running in a ribbon.
 * When caller specify ribbon_id = -1, ribbon_thread_create automatically creates a new ribbon 
 * for the about-to-run thread to running in.  Without non-zero ribbon, the function first check
 * if the ribbon_id exists in the system,  then proceed to create the thread to run in the given
 * ribbon id.
 * Return the ribbon_id the new thread is running in. On error, return -1.
 * If defined as pthread_create, we should return 0 but not the ribbon id.
 */
int ribbon_thread_create(int ribbon_id, pthread_t *tid, void *(fn)(void*), void *args){
	int rv = 0;
	char buf[100];
	int memdom_id;
    pthread_attr_t attr;    
	void *stack_base;
	unsigned long stack_size;

	/* When caller specify ribbon_id = -1, ribbon_thread_create automatically creates a new ribbon 
	 * for the about-to-run thread to running in. 
	 */
	if (ribbon_id == NEW_RIBBON) {
		ribbon_id = ribbon_create();
		fprintf(stderr, "creating a new ribbon %d for the new thread to run in\n", ribbon_id);
	}

	/* Block thread if it tries to run in a non-existing ribbon */
	if (!ribbon_exists(ribbon_id)) {
		fprintf(stderr, "thread cannot run in a non-existing ribbon %d\n", ribbon_id);		
		return -1;
	}

	/* Join the global memdom if the main thread allows all threads to access the global memory areas */
	if( ALLOW_GLOBAL ){
		ribbon_join_domain(0, ribbon_id);
		memdom_priv_add(0, ribbon_id, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_ALLOCATE | MEMDOM_EXECUTE);
	}

	/* Atomic operation */
	pthread_mutex_lock(&create_thread_mutex);

	pthread_attr_init(&attr);
#ifdef THREAD_PRIVATE_STACK // Use private stack for thread 
	/* Create a thread-local memdom and make ribbon join it */
	memdom_id = memdom_create();
	if (memdom_id == -1) {
		fprintf(stderr, "failed to create thread local memdom for ribbon %d\n", ribbon_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}
	/* Join this newly created memdom for this ribbon */
	ribbon_join_domain(memdom_id, ribbon_id);
	memdom_priv_add(memdom_id, ribbon_id, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_ALLOCATE | MEMDOM_EXECUTE);

	/* Make the main thread join this new memdom in order to set up the stack properly */
	ribbon_join_domain(memdom_id, 0);
	memdom_priv_add(memdom_id, 0, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_ALLOCATE | MEMDOM_EXECUTE);

	/* Setup thread local stack 
	 * Here we are using mmap for the newly created memdom, no contention is possible, so don't lock memdom lock 
	 */
	stack_size = PTHREAD_STACK_MIN + 0x8000;
	stack_base = (void*) memdom_mmap(memdom_id, 0, stack_size, PROT_READ | PROT_WRITE, 
									 MAP_PRIVATE | MAP_ANONYMOUS | MAP_MEMDOM, 0, 0);	
	if (stack_base == MAP_FAILED) {
		perror("mmap for thread stack: ");
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}
	pthread_attr_setstack(&attr, stack_base, stack_size);
	printf("[%s] creating thread with stack base: %p, end: 0x%lx\n", __func__, stack_base, (unsigned long)stack_base + stack_size);

	/* Record thread-private memdom addr and size */
	memdom[memdom_id]->start = stack_base;
	memdom[memdom_id]->total_size = stack_size;

#endif // THREAD_PRIVATE_STACK

	/* Tell the kernel we are going to create a pthread, that is actually an smv thread
	 * The kernel will set mm->standby_ribbon_id = ribbon_id */
	sprintf(buf, "ribbon,registerthread,%d", ribbon_id);
	rv = message_to_kernel(buf);
	if ( rv != 0) {
		fprintf(stderr, "register_ribbon_thread for ribbon %d failed\n", ribbon_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}

#ifdef INTERCEPT_PTHREAD_CREATE
#undef pthread_create
#endif
	/* Create a pthread (kernel knows it's a ribbon thread because we registered a ribbon id for this thread */
	/* Use the real pthread_create */
	rv = pthread_create(tid, &attr, fn, args);
	if (rv) {
		fprintf(stderr, "pthread_create for ribbon %d failed\n", ribbon_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}
	fprintf(stderr, "ribbon %d is ready to run\n", ribbon_id);

#ifdef INTERCEPT_PTHREAD_CREATE
	/* Set return value to 0 to avoid pthread_create error */
	ribbon_id = 0;
	/* ReDefine pthread_create to be ribbon_thread_create again */
#define pthread_create(tid, attr, fn, args) ribbon_thread_create(NEW_RIBBON, tid, fn, args)
#endif

#ifdef THREAD_PRIVATE_STACK
	/* Main thread should leave the thread's memdom after the setup */
	ribbon_leave_domain(memdom_id, 0);
#endif

	pthread_mutex_unlock(&create_thread_mutex);

	return ribbon_id;
}
