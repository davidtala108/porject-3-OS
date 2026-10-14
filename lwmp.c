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
context current_context;
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
	thread_context->state.rsi = function;
	thread_context->state.rdi = argument;
	thread_context->state.rsp = stack;
	thread_context->state.rbp = stack;
	thread_context->status = LWP_RUNNING;
	thread_context->lib_one = NULL;
	thread_context->lib_two = NULL;
	thread_context->sched_one = NULL;
	thread_context->sched_two = NULL;
	thread_context->exited = NULL;
	stack.push(lwp_wrap(function, argument);
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



        count++;
        OG_context->stack = NULL;
        OG_context->tid = NULL;
        OG_context->stacksize = NULL;
        OG_context->state.fxsave = NULL;
	OG_context->state.rsi = NULL;
	OG_context->state.rdi = NULL;
	OG_context->state.rsp = NULL;
	OG_context->state.rbp = NULL;
        OG_context->status = NULL;
        OG_context->lib_one = NULL;
        OG_context->lib_two = NULL;
        OG_context->sched_one = NULL;
        OG_context->sched_two = NULL;
        OG_context->exited = NULL;
	
	RoundRobin->amit(OG_context);
	current_context = OG_context;
	lwp_yield();

}

/* Yield control to another LWP */
void lwp_yield(void) {
   	context *old_lwp = current_context;
	context *next_lwp = RoundRobin->next();
	
        if (next_lwp !=NULL && next_lwp != old_lwp){
	   current_context = next_lwp;
           swap_rfiles(old_lwp->state, next_lwp.state);
        }else{
	printf("Terminating Program");
	return;
	}


}

/* Terminate the calling LWP */
void lwp_exit(int status) {
    current_context->status = LWP_TERMINATED;
    current_context->exited = status;
    RoundRobin->remove(current_context);
    
    context *new = RoundRobin->next();
     if (new !=NULL && new != current_context){
	   context *old_lwp = current_context;
           current_context = next_lwp;
           swap_rfiles(old_lwp->state, next_lwp.state);
        }else{
        printf("Terminating Program");
        return;
        }
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

static void lwp_wrap(lwpfun fun, void* arg) {
/*
PA3.md 2026-10-07
9 / 16
* Call the given function with the given argument
* Call lwp_exit() with its return value
*/
int rval;
rval = fun(arg);
lwp_exit(rval);

}
