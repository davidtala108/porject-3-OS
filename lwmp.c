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

// two seperate qeuees for the teminated and waiting threads
static thread terminated_head = NULL;
static thread terminated_tail = NULL;
static thread waiting_head = NULL;
static thread waiting_tail = NULL;

// global variable for the first thread in a linked list of threads
static thread all_threads = NULL;

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

// helper function that we will need in the exit function that lets us put a thread in either of the two queues that are defined above
static void enqueue(thread *head, thread *tail, thread threa) {

    // adding the new thread "threa" to the end of the queue
    // since it's the last thread, its lib_two pointer should be NULL
    threa->lib_two = NULL; // lib_two is provided in lwp.h and is basically thread.next (or at least that's how I'm using it)

    if (*tail == NULL) {
        *head = threa;
    } else {
        (*tail)->lib_two = threa;
    }

    *tail = threa;
}

// helper function that is practically the opposite as the above function
static thread dequeue(thread *head, thread *tail) {

    if (*head == NULL) {
        return NULL;
    }

    thread removed = *head;

    *head = removed->lib_two;

    if (*head == NULL) {
        *tail = NULL;
    }

    removed->lib_two = NULL;

    return removed;
}

// we will want this case for when the lwp_wait() function frees a thread
// If we never remove it from the global list, then tid2thread() might try and access global memory
static void remove_thread(thread removed) {

    thread temp = all_threads;
    thread prev = NULL;

    while (temp != NULL) {

        if (temp == removed) {

            if (prev == NULL) {
                all_threads = temp->lib_one;
            } else {
                prev->lib_one = temp->lib_one;
            }

            return;
        }

        prev = temp;
        temp = temp->lib_one;
    }
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
    context *thread_context = calloc(1, sizeof(*thread_context));

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

    thread_context->exited = NULL;
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

    // put all threads into the global thread list that is used in tid2thread()
    thread_context->lib_one = all_threads;
    all_threads = thread_context;

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
    context *context = calloc(1, sizeof(*context));

    if (context == NULL) {
        fprintf(stderr, "Malloc failed\n");
        return;
    }

	// assign a thread if
    if (next_tid == NO_THREAD || next_tid == ULONG_MAX) {
        free(context);
        return;
    }

    context->exited = NULL;
    context->tid = next_tid++;
    context->stack = NULL;
    context->stacksize = 0;
    context->status = LWP_LIVE;

    // we do this to make sure that we put the thread that's currently in the system into our global list
    context->lib_one = all_threads;
    all_threads = context;

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

    // check if any threads are waiting
    if (waiting_head != NULL) {

        // get the oldest waiting thread
        thread waiting = dequeue(&waiting_head, &waiting_tail);

        // associate the terminated thread with the waiting thread
        waiting->exited = old_lwp;

        // put the waiting thread back into the scheduler
        get_scheduler()->admit(waiting);

    } else {
        // in this case, no threads are waiting, so save the terminated thread
        enqueue(&terminated_head, &terminated_tail, old_lwp);
    }

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

    if (current_context == NULL) {
        return NO_THREAD;
    }
    
    // check if a thread has been terminated
    if (terminated_head != NULL) {

        // remove the oldest terminated thread
        thread finished = dequeue(&terminated_head, &terminated_tail);

        // save its thread ID
        tid_t id = finished->tid;

        // return its termination status if requested
        if (status != NULL) {
            *status = finished->status;
        }

        // free its allocated stack
        if (finished->stack != NULL) {
            munmap(finished->stack, finished->stacksize);
        }

        // free the thread context (free the heap! (or stack in this case))
        remove_thread(finished); // remove from the global lsit of threads too
        free(finished);

        return id;
    }

    // check if any other runnable threads exist
    if (get_scheduler()->qlen() <= 1) {
        return NO_THREAD;
    }

    // now we're in the position where we want to block the calling thread

    // save the currently running thread and add it to the waiting queue
    thread waiting = current_context;
    get_scheduler()->remove(waiting);
    enqueue(&waiting_head, &waiting_tail, waiting);

    // select the next runnable thread and update the currently running thread
    thread next = get_scheduler()->next();

    current_context = next;

    // switch execution to the next thread (the context switch)
    swap_rfiles(&waiting->state, &next->state);

    // get the thread that terminated while we were waiting
    thread finished = waiting->exited;

    tid_t id = finished->tid;

    // collect the termination status
    if (status != NULL) {
        *status = finished->status;
    }

    // free its stack 
    if (finished->stack != NULL) {
        munmap(finished->stack, finished->stacksize);
    }

    // free its context (free the heap!)
    remove_thread(finished); // remove from global lsit too
    free(finished);

    return id;
}

/* Return the ID of the calling LWP */ // (lowkey the easiest to implement David (besides maybe get scheduler))
tid_t lwp_gettid(void) {

    if (current_context == NULL) {
        return NO_THREAD;
    }

    return current_context->tid;
}

/* Find a thread by its ID */
thread tid2thread(tid_t tid) {

    thread temp = all_threads;

    while (temp != NULL) {
        if (temp->tid == tid) {
            return temp;
        }

        temp = temp->lib_one;
    }

    return NULL;
}

/* Install a scheduler */
void lwp_set_scheduler(scheduler schedulerr) {

    scheduler old = get_scheduler();

    if (schedulerr == NULL) {
        schedulerr = RoundRobin;
    }

    if (schedulerr == old) {
        return;
    }

    if (schedulerr->init != NULL) {
        schedulerr->init();
    }

    while (old->qlen() > 0) {

        thread temp = old->next();

        old->remove(temp);
        schedulerr->admit(temp);
    }

    if (old->shutdown != NULL) {
        old->shutdown();
    }

    current_scheduler = schedulerr;
    scheduler_initialized = 1;
}

/* Return the current scheduler */
scheduler lwp_get_scheduler(void) {

    return get_scheduler();
}