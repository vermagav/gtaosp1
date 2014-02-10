#include "gtthread.h"

void testPrint() {
	printf("Hello from test thread!");
}

int main(int argc, char **argv) {
	
	long period = 10;
	gtthread_init(period);

	struct gtthread_t test1;

	gtthread_create(test1, testPrint, 0);

	// TODO


	return 0;
}
