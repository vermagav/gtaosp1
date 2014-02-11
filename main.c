#include <stdio.h>
#include <stdlib.h>
#include "gtthread.h"

void *thr1(void *in) {
    printf("Hello World!\n");
    fflush(stdout);
    return NULL;
}

int main(int argc, char **argv) {
    gtthread_t t1;

    gtthread_init(200);
    gtthread_create(&t1, thr1, NULL);
    gtthread_create(&t1, thr1, NULL);
    gtthread_create(&t1, thr1, NULL);

	gtthread_print_all();

    while(1);

    return EXIT_SUCCESS;
}
