#define _GNU_SOURCE

#include "lwp.h"
#include "rr.h"
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define LWP_RUNNING   0
#define LWP_TERMINATED 1

int count = 0;
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

	if (thread_context == NULL){
		printf("Malloc failed");
		return 1;
	}
	count++;
	thread_context->stack = stack;
       	thread_context->tid = count;
	thread_context->stacksize = stack_size;
	thread_context->state.fxsave = FPU_INIT;
	thread_context->status = LWP_RUNNING;
	thread_context->lib_one = NULL;
	thread_context->lib_two = NULL;
	thread_context->sched_one = NULL;
	thread_context->sched_two = NULL;
	thread_context->exited = NULL;
	
	RoundRobin->admit(thread_context);
	return  thread_context->tid;	

}

/* Start the LWP system */
void lwp_start(void) {
    context *OG_context = malloc(sizeof(context));

        if (OG_context == NULL){
                printf("Malloc failed");
                return 1;
        }

	void *stack;

__asm__ volatile ("mov %%rsp, %0" : "=r"(stack));

        count++;
        OG_context->stack = stack;
        OG_context->tid = count;
        OG_context->stacksize = stack_size;
        OG_context->state.fxsave = FPU_INIT;
        OG_context->status = LWP_RUNNING;
        OG_context->lib_one = NULL;
        OG_context->lib_two = NULL;
        OG_context->sched_one = NULL;
        OG_context->sched_two = NULL;
        OG_context->exited = NULL;:

        RoundRobin->admit(OG_context);
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
