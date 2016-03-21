#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "ribbon_lib.h"

#define RIBBON_CLONE_FLAG (CLONE_RIBBON |CLONE_FS | CLONE_FILES | SIGCHLD) 

pthread_mutex_t create_thread_mutex;

/* Telling the kernel that this process will be using the secure memory view model 
 * The master thread must call this routine to notify the kernel its status */
int ribbon_main_init(void){
    int rv = -1;

	/* Set mm->using_smv to true in kernel space */
	rv = message_to_kernel("ribbon,maininit");
	if (rv != 0) {
		fprintf(stderr, "ribbon_main_init() failed\n");
		return -1;
	}
    rlog("kernel responded %d\n", rv);

	/* Initialize mutex for protecting smv_thread_create */
	pthread_mutex_init(&create_thread_mutex, NULL);

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

/* Create an smv thread running in a ribbon, return 0 on success, -1 otherwise */
int ribbon_thread_create(int ribbon_id, pthread_t *tid, void *(fn)(void*), void *args){
	int rv = 0;
	char buf[100];

	/* Atomic operation */
	pthread_mutex_lock(&create_thread_mutex);

	/* Tell the kernel we are going to create a pthread, that is actually an smv thread
	 * The kernel will set mm->standby_ribbon_id = ribbon_id */
	sprintf(buf, "ribbon,registerthread,%d", ribbon_id);
	rv = message_to_kernel(buf);
	if ( rv != 0) {
		fprintf(stderr, "register_ribbon_thread for ribbon %d failed\n", ribbon_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}

	/* Create a pthread (kernel knows it's a ribbon thread because we registered a ribbon id for this thread */
	rv = pthread_create(tid, NULL, fn, args);
	if (rv) {
		fprintf(stderr, "pthread_create for ribbon %d failed\n", ribbon_id);		
		pthread_mutex_unlock(&create_thread_mutex);
		return -1;
	}

	pthread_mutex_unlock(&create_thread_mutex);

	return 0;
}
