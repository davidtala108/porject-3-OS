
#define _GNU_SOURCE

#include "lwp.h"
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* Create a new lightweight process */
tid_t lwp_create(lwpfun function, void *argument) {
	size_t stack_size = 8 * 1024 * 1024;
    	unsigned long *stack;
	stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE |
		MAP_ANONYMOUS | MAP_STACK, -1, 0);
	
	if (stack == MAP_FAILED){
		printf("Building stack failed");
		return 1;
	}

	context *thread_context = malloc(sizeof(context));

	if (context == NULL){
		printf("Malloc failed");
		return 1;
	}

	thread_context->stack = stack
       	thread_context->	
}

/* Start the LWP system */
void lwp_start(void) {
    // TODO
}

/* Yield control to another LWP */
void lwp_yield(void) {
    // TODO
}

/* Terminate the calling LWP */
void lwp_exit(int status) {
    // TODO
}

/* Wait for a terminated LWP */
tid_t lwp_wait(int *status) {
    // TODO
}

/* Return the ID of the calling LWP */
tid_t lwp_gettid(void) {
    // TODO
}

/* Find a thread by its ID */
thread tid2thread(tid_t tid) {
    // TODO
}

/* Install a scheduler */
void lwp_set_scheduler(scheduler sched) {
    // TODO
}

/* Return the current scheduler */
scheduler lwp_get_scheduler(void) {
    // TODO
}
