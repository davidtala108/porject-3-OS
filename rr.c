#include "lwp.h"
#include <stddef.h>

// linked list of runnable threads
static thread head = NULL;

// next thread to be selected
static thread current = NULL;

// number of runnable threads
static int threadCount = 0;

// initialize the round robin scheduler
void rr_init(void) {
    head = NULL;
    current = NULL;
    threadCount = 0;
}

// clean up the scheduler
void rr_shutdown(void) {
    head = NULL;
    current = NULL;
    threadCount = 0;
}

// add a new thread to the scheduler
void rr_admit(thread newThread) {

    if (newThread == NULL) {
        return;
    }

    // first thread in the scheduler
    if (head == NULL) {
        head = newThread;
        current = newThread;

        newThread->sched_one = newThread;
        newThread->sched_two = newThread;
    } else {

        // get the last thread
        thread last = head->sched_two;

        // insert the new thread at the end
        last->sched_one = newThread;
        newThread->sched_two = last;

        // maintain the circular linked list
        newThread->sched_one = head;
        head->sched_two = newThread;
    }

    threadCount++;
}

// remove a thread from the scheduler
void rr_remove(thread removed) {

    if (removed == NULL || head == NULL) {
        return;
    }

    // find the thread in the list
    thread temp = head;
    int found = 0;
    int checked = 0;

    while (checked < threadCount) {

        if (temp == removed) {
            found = 1;
            break;
        }

        temp = temp->sched_one;
        checked++;
    }

    if (!found) {
        return;
    }

    // only one thread in the scheduler
    if (threadCount == 1) {
        head = NULL;
        current = NULL;
    } else {

        thread previous = removed->sched_two;
        thread next = removed->sched_one;

        // connect the neighboring threads
        previous->sched_one = next;
        next->sched_two = previous;

        // update head if needed
        if (head == removed) {
            head = next;
        }

        // update next scheduled thread if needed
        if (current == removed) {
            current = next;
        }
    }

    // disconnect the removed thread
    removed->sched_one = NULL;
    removed->sched_two = NULL;

    threadCount--;
}

// return the next thread to execute
thread rr_next(void) {

    if (current == NULL) {
        return NULL;
    }

    thread nextThread = current;

    // go to the following thread
    current = current->sched_one;

    return nextThread;
}

// return the number of runnable threads
int rr_qlen(void) {
    return threadCount;
}

// define and expose the round robin scheduler
struct scheduler rr_publish = {
    rr_init,
    rr_shutdown,
    rr_admit,
    rr_remove,
    rr_next,
    rr_qlen
};

scheduler RoundRobin = &rr_publish;
