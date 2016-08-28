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
#include "smv_lib.h"

pthread_mutex_t create_thread_mutex;
int ALLOW_GLOBAL; // 1: all threads can access global memdom, 0 otherwise

/* Telling the kernel that this process will be using the secure memory view model 
 * The master thread must call this routine to notify the kernel its status */
int smv_main_init(int global){
    int rv = -1;
	ALLOW_GLOBAL = 0;

	/* Set mm->using_smv to true in kernel space */
	rv = message_to_kernel("smv,maininit");
	if (rv != 0) {
		fprintf(stderr, "smv_main_init() failed\n");
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

/* Create a smv and return the ID of the new smv */
int smv_create(void) {
    int smv_id = -1;
	smv_id = message_to_kernel("smv,create");
	if (smv_id < 0) {
		fprintf(stderr, "smv_create() failed\n");
		return -1;
	}
    rlog("kernel responded smv id %d\n", smv_id);
	return smv_id;
}

/* Destroy the smv smv_id */
int smv_kill(int smv_id) {
	int rv = 0;
	char buf[100];
	sprintf(buf, "smv,kill,%d", smv_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "smv_kill(%d) failed\n", smv_id);
		return -1;
	}
	rlog("smv ID %d killed", smv_id);
	return rv;
}

/* Add smv to memory domain */
int smv_join_domain(int memdom_id, int smv_id) {
	int rv = 0;
	char buf[50];
	sprintf(buf, "smv,domain,%d,join,%d", smv_id, memdom_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "smv_join_domain(smv %d, memdom %d) failed\n", smv_id, memdom_id);
		return -1;
	}
	rlog("smv ID %d joined memdom ID %d", smv_id, memdom_id);
	return 0;
}

/* Remove smv smv_id from memory domain memdom */
int smv_leave_domain(int memdom_id, int smv_id) {
	int rv = 0;
	char buf[100];
	sprintf(buf, "smv,domain,%d,leave,%d", smv_id, memdom_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "smv_leave_domain(smv %d, memdom %d) failed\n", smv_id, memdom_id);
		return -1;
	}
	rlog("smv ID %d left memdom ID %d", smv_id, memdom_id);
    return rv;
}

/* Check if smv is in memory domain */
int smv_is_in_domain(int memdom_id, int smv_id) {
	int rv = 0;
	char buf[50];
	sprintf(buf, "smv,domain,%d,isin,%d", smv_id, memdom_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "smv_is_in_domain(smv %d, memdom %d) failed\n", smv_id, memdom_id);
		return -1;
	}
	rlog("smv ID %d in memdom ID %d?: %d", smv_id, memdom_id, rv);
	return rv;
}

/* Check if smv is in memory domain */
int smv_exists(int smv_id) {
	int rv = 0;
	char buf[50];
	sprintf(buf, "smv,exists,%d", smv_id);
	rv = message_to_kernel(buf);
	if (rv == -1) {
		fprintf(stderr, "smv_exists(smv %d) failed\n", smv_id);
		return -1;
	}
	rlog("smv ID %d exists? %d", smv_id, rv);
	return rv;
}

/* Create an smv thread running in a smv.
 * When caller specify smv_id = -1, smvthread_create automatically creates a new smv 
 * for the about-to-run thread to running in.  Without non-zero smv, the function first check
 * if the smv_id exists in the system,  then proceed to create the thread to run in the given
 * smv id.
 * Return the smv_id the new thread is running in. On error, return -1.
 * If defined as pthread_create, we should return 0 but not the smv id.
 */
int smvthread_create(int smv_id, pthread_t *tid, void *(fn)(void*), void *args){
	int rv = 0;
	char buf[100];
	int memdom_id;
    pthread_attr_t attr;    
	void *stack_base;
	unsigned long stack_size;

	/* When caller specify smv_id = -1, smvthread_create automatically creates a new smv 
	 * for the about-to-run thread to running in. 
	 */
	if (smv_id == NEW_SMV) {
		smv_id = smv_create();
		fprintf(stderr, "creating a new smv %d for the new thread to run in\n", smv_id);
	}

	/* Block thread if it tries to run in a non-existing smv */
	if (!smv_exists(smv_id)) {
		fprintf(stderr, "thread cannot run in a non-existing smv %d\n", smv_id);		
		return -1;
	}

	/* Join the global memdom if the main thread allows all threads to access the global memory areas */
	if( ALLOW_GLOBAL ){
		smv_join_domain(0, smv_id);
		memdom_priv_add(0, smv_id, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_ALLOCATE | MEMDOM_EXECUTE);
	}

	/* Atomic operation */
	pthread_mutex_lock(&create_thread_mutex);

	pthread_attr_init(&attr);
#ifdef THREAD_PRIVATE_STACK // Use private stack for thread 
	/* Create a thread-local memdom and make smv join it */
	memdom_id = memdom_create();
	if (memdom_id == -1) {
		fprintf(stderr, "failed to create thread local memdom for smv %d\n", smv_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}
	/* Join this newly created memdom for this smv */
	smv_join_domain(memdom_id, smv_id);
	memdom_priv_add(memdom_id, smv_id, MEMDOM_READ | MEMDOM_WRITE | MEMDOM_ALLOCATE | MEMDOM_EXECUTE);

	/* Make the main thread join this new memdom in order to set up the stack properly */
	smv_join_domain(memdom_id, 0);
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
	 * The kernel will set mm->standby_smv_id = smv_id */
	sprintf(buf, "smv,registerthread,%d", smv_id);
	rv = message_to_kernel(buf);
	if ( rv != 0) {
		fprintf(stderr, "register_smv_thread for smv %d failed\n", smv_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}

#ifdef INTERCEPT_PTHREAD_CREATE
#undef pthread_create
#endif
	/* Create a pthread (kernel knows it's a smv thread because we registered a smv id for this thread */
	/* Use the real pthread_create */
	rv = pthread_create(tid, &attr, fn, args);
	if (rv) {
		fprintf(stderr, "pthread_create for smv %d failed\n", smv_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}
	fprintf(stderr, "smv %d is ready to run\n", smv_id);

#ifdef INTERCEPT_PTHREAD_CREATE
	/* Set return value to 0 to avoid pthread_create error */
	smv_id = 0;
	/* ReDefine pthread_create to be smvthread_create again */
#define pthread_create(tid, attr, fn, args) smvthread_create(NEW_SMV, tid, fn, args)
#endif

#ifdef THREAD_PRIVATE_STACK
	/* Main thread should leave the thread's memdom after the setup */
	smv_leave_domain(memdom_id, 0);
#endif

	pthread_mutex_unlock(&create_thread_mutex);

	return smv_id;
}
