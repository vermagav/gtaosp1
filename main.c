#include "gtthread.h"

void *testPrint() {
	printf("Hello from the thread!\n");
}

int main(int argc, char **argv) {
	
	long period = 200;
	gtthread_init(period);

	struct gtthread_t test1;

	gtthread_create(&test1, testPrint, NULL);
	gtthread_create(&test1, testPrint, NULL);
	gtthread_create(&test1, testPrint, NULL);

	gtthread_print_all();

	while(1);
	// TODO


	return 0;
}
