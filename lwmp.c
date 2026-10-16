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
#include <stdint.h>
#include <limits.h>

static thread current_context = NULL; // a pointer to the currently running thread
static tid_t next_tid = 1; // the id of the next available thread
static scheduler current_scheduler = NULL; // pointer to the structure with the scheduler fucntions (rr by default)
static int scheduler_initialized = 0; // flag that determines if we initialized the scheduler

/* Wrapper for thread functions */
static void lwp_wrap(lwpfun fun, void *arg) {

    int rval = fun(arg);

    lwp_exit(rval);
}

// gets the current scheduler
static scheduler get_scheduler(void) {
	// checks if a scheduler exists and sets to rr by default
    if (current_scheduler == NULL) {
        current_scheduler = RoundRobin;
    }

	// initializes the scheduler if it has not been already
    if (!scheduler_initialized) {
        if (current_scheduler->init != NULL) {
            current_scheduler->init();
        }

        scheduler_initialized = 1;
    }

    return current_scheduler;
}

/* Create a new lightweight process */
tid_t lwp_create(lwpfun function, void *argument) {

	// declaring the stack size
    struct rlimit limit;
    size_t stack_size = 8 * 1024 * 1024;

	// check for valid function pointer that will be provided by the user
    if (function == NULL) {
        return NO_THREAD;
    }

	// checking the systems stack limit
    if (getrlimit(RLIMIT_STACK, &limit) == 0 &&
        limit.rlim_cur != RLIM_INFINITY &&
        limit.rlim_cur > 0) {

        if (limit.rlim_cur > SIZE_MAX) {
            return NO_THREAD;
        }

        stack_size = (size_t)limit.rlim_cur;
    }

	// get the page size 
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size <= 0) {
        return NO_THREAD;
    }
    size_t page = (size_t)page_size;

	// roudn the page size up to a page multiple
    if (stack_size > SIZE_MAX - (page - 1)) {
        return NO_THREAD;
    }
    stack_size = ((stack_size + page - 1) / page) * page;

	// make sure the stack is large enough
    if (stack_size < 4 * sizeof(unsigned long)) {
        return NO_THREAD;
    }

	// allocation of memory using mmap
    unsigned long *stack = mmap(
        NULL,
        stack_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        -1,
        0
    );

    if (stack == MAP_FAILED) {
        return NO_THREAD;
    }

	// allocate memory for the thread context
    context *thread_context = malloc(1, sizeof(context));

    if (thread_context == NULL) {
        munmap(stack, stack_size);
        return NO_THREAD;
    }

	// assigned a unique thread id
    if (next_tid == NO_THREAD || next_tid == ULONG_MAX) {
        munmap(stack, stack_size);
        free(thread_context);
        return NO_THREAD;
    }

    thread_context->tid = next_tid++;
    thread_context->stack = stack;
    thread_context->stacksize = stack_size;
    thread_context->status = LWP_LIVE;

    thread_context->state.fxsave = FPU_INIT;

    // args for lwp_wrap (function, argument)
    thread_context->state.rdi = (unsigned long)function;
    thread_context->state.rsi = (unsigned long)argument;

    // stack grows downward from high memory
    unsigned long *sp = (unsigned long *)((char *)stack + stack_size);

    // build the initial stack frame for swap_rfiles()
	// at entry to lwp_wrap(), rsp must be 8 mod 16.

    *(--sp) = 0;
    *(--sp) = (unsigned long)lwp_wrap;
    *(--sp) = 0;

    thread_context->state.rbp = (unsigned long)sp;
    thread_context->state.rsp = (unsigned long)sp;

	// put the thread context into the scheduler
    get_scheduler()->admit(thread_context);

    return thread_context->tid;
}

/* Start the LWP system */
void lwp_start(void) {

    if (current_context != NULL) {
        return;
    }

	// context allocation for current thread
    context *context = malloc(1, sizeof(context));

    if (context == NULL) {
        fprintf(stderr, "Malloc failed\n");
        return;
    }

	// assign a thread if
    if (next_tid == NO_THREAD || next_tid == ULONG_MAX) {
        free(OG_context);
        return;
    }

    context->tid = next_tid++;
    context->stack = NULL;
    context->stacksize = 0;
    context->status = LWP_LIVE;

	// sets original thread to the current thread
    current_context = context;

	// put og thread into scheduler
    get_scheduler()->admit(context);

	// give control to the scheduler as required
    lwp_yield();
}

/* Yield control to another LWP */
void lwp_yield(void) {

    if (current_context == NULL) {
        return;
    }

	// get pointer of current thread
    thread old_lwp = current_context;

	// requests from the scheduler which thread should go next
    thread next_lwp = get_scheduler()->next();

    if (next_lwp == NULL) {
        exit(0);
    }

    if (next_lwp == old_lwp) {
        return;
    }

    current_context = next_lwp;

	// perfoems the actual context switch
    swap_rfiles(&old_lwp->state, &next_lwp->state);
}

/* Terminate the calling LWP */
void lwp_exit(int status) {

    if (current_context == NULL) {
        exit(status & 0xFF);
    }

	// pointer to the current thread
    thread old_lwp = current_context;

	// provided in lwp.h (updates the status)
    old_lwp->status = MKTERMSTAT(LWP_TERM, status);

	// rmeoval of thread
    get_scheduler()->remove(old_lwp);

	// requests from the scheduler which thread should go next
    thread next_lwp = get_scheduler()->next();

    if (next_lwp == NULL) {
        exit(0);
    }

    current_context = next_lwp;

    // the exiting thread never resumes and we swap to the new thread
    swap_rfiles(NULL, &next_lwp->state);

    // this should never be reached (in theory) but makes sure we never return
    abort();
}

/* Wait for a terminated LWP */
tid_t lwp_wait(int *status) {
    // TODO
    return NO_THREAD;
}

/* Return the ID of the calling LWP */
tid_t lwp_gettid(void) {
    // TODO
    return NO_THREAD;
}

/* Find a thread by its ID */
thread tid2thread(tid_t tid) {
    // TODO
    return NULL;
}

/* Install a scheduler */
void lwp_set_scheduler(scheduler sched) {
    // TODO
    (void)sched;
}

/* Return the current scheduler */
scheduler lwp_get_scheduler(void) {
    // TODO
    return NULL;
}