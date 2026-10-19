
#include <stdio.h>
#include "lwp.h"

int test_thread(void *arg) {

    int num = *(int *)arg;

    for (int i = 0; i < 3; i++) {
        printf("Thread %d running\n", num);
        lwp_yield();
    }

    return num;
}

int main(void) {

    int a = 1;
    int b = 2;

    lwp_create(test_thread, &a);
    lwp_create(test_thread, &b);

    lwp_start();

    return 0;
}
