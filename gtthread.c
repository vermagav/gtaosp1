#include "gtthread.h"
#include "gtthread_common.h"

//#define _XOPEN_SOURCE 600 // TODO: remove

#include <signal.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/time.h> 
#include <ucontext.h>


/**********************************
 * A single thread's data structure
 **********************************/


typedef struct gtthread_t {
	// Thread ID
	int tid;
	
	// Function that it points to with its arguments and return value
	void *(*start_routine)(void *);
	void *arg;
	void *retval;

	bool deleted;

	// Context for this thread, used while switching
	ucontext_t *context;

	// Pointer macro used by TAILQ queue implemenetation
	TAILQ_ENTRY(gtthread_t) entries;
} gtthread_t;


/*******************************************
 * Global variables used throughout gtthread
 *******************************************/


// Timeslice for each thread's execution
long gtthread_period = -1;
// Maintain a global thread count
int gtthread_count = 0;
// Used for assigning new thread IDs, this never decrements
int gtthread_id = 0;
// Stack size of 8 KB for each context
const int CONTEXT_STACK_SIZE = 8192;
// The head of the queue that holds our threads
// @Citation: Usage of TAILQ inspired from http://blog.jasonish.org/2006/08/tailq-example.html
TAILQ_HEAD(, gtthread_t) queue_head;
// Other
gtthread_t *main_thread;
ucontext_t *main_context;
bool initialized = false;


/***********************************
 * Function definitions for gtthread
 ***********************************/


// This is a debugging function
void gtthread_print_all() {
    gtthread_t *thread;
    TAILQ_FOREACH(thread, &queue_head, entries) {
        printf("Thread found with id: %d", thread->tid);
        if(thread->tid == 0) {
        	printf(" (main_thread)");
        }
        printf("\n");
    }
}

// We need a wrapper function to store return value
void* gtthread_wrapper(void *start_routine(), void *arg) {
	printf("test\n");
	gtthread_t *thread = TAILQ_FIRST(&queue_head);
	printf("Reached inside wrapper function for thread %d\n", thread->tid); // TODO TAILQ_REMOVE
	thread->retval = start_routine(arg);
	printf("Wrapper start routine called!\n"); // TODO remove
	thread->deleted = true;
	return;
}

// Our swapping/scheduling function
void gtthread_next() {
	// Swap context from the head of the queue to the next
	gtthread_t *head_thread = TAILQ_FIRST(&queue_head);
	gtthread_t *next = TAILQ_NEXT(head_thread, entries);
	swapcontext(head_thread->context, next->context);

	// Move the head to the tail of the queue
	TAILQ_REMOVE(&queue_head, head_thread, entries);
	TAILQ_INSERT_TAIL(&queue_head, head_thread, entries);
	//printf("Swapped thread id %d with %d\n", head_thread->tid, next->tid); // TODO remove
}

void gtthread_init(long period) {
	if(initialized) {
		fatal_error("Error: gtthreads already initialized!");
	}

	printf("Reached 1\n");
	// Set round robin cycle period
	gtthread_period = period;
	
	// Declare variables for signal timer usage
	// @Citation: Snippet taken from http://www.makelinux.net/alp/069
	struct sigaction sa;
	struct itimerval timer;
	
	// Initialize queue
	TAILQ_INIT(&queue_head);
	printf("Reached 2\n");

	// Since gtthread_init() is called only once, create the parent main_thread
	printf("Reached 3\n");
	// Allocate memory for thread and populate it
	main_thread = (gtthread_t*)malloc(sizeof(gtthread_t));
	ucontext_t new_context;
	main_thread->context = &new_context;
	main_thread->tid = gtthread_id++;
	main_thread->deleted = false;
	gtthread_count++;
	printf("Reached 4\n");
	// Create context for this thread of specified size
	if(getcontext(main_thread->context) == -1) {
		fatal_error("Error: getcontext() failed while initializing!");
	}
	printf("Reached 5\n");
	main_thread->context->uc_stack.ss_sp = malloc(CONTEXT_STACK_SIZE);
	main_thread->context->uc_stack.ss_size = CONTEXT_STACK_SIZE;
	main_thread->context->uc_stack.ss_flags = 0;
	main_thread->context->uc_link = NULL;
	printf("Reached 6\n");
	// Insert main thread to queue, main thread has id 0
	TAILQ_INSERT_TAIL(&queue_head, main_thread, entries);

	// Install the gtthread_next() function for the signal as its action
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = gtthread_next;
	sigaction(SIGVTALRM, &sa, NULL);
	printf("Reached 7\n");
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = period;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = period;

	setitimer(ITIMER_VIRTUAL, &timer, NULL);
	printf("Reached 8\n");
	// All done!
	initialized = true;
	printf("gtthread initialized\n"); // TODO: remove
}

int gtthread_create(gtthread_t *thread, void *(*start_routine)(void *), void *arg) {
	if(!initialized) {
		fatal_error("Error: tried to create thread without initializing!");
	}

	// Allocate memory for thread and populate it
	thread = (gtthread_t*)malloc(sizeof(gtthread_t));
	thread->start_routine = start_routine;
	thread->arg = arg;
	thread->tid = gtthread_id++;
	thread->deleted = false;
	gtthread_count++;

	// Create context for this thread of specified size
	ucontext_t new_context;
	thread->context = &new_context;

	if(getcontext(thread->context) == -1) {
		fatal_error("Error: getcontext() failed while creating thread!");
	}

	thread->context->uc_stack.ss_sp = malloc(CONTEXT_STACK_SIZE);
	thread->context->uc_stack.ss_size = CONTEXT_STACK_SIZE;
	thread->context->uc_stack.ss_flags = 0;
	thread->context->uc_link = main_context;

	// Initialize the context for this thread
	makecontext(thread->context, (void(*)(void)) gtthread_wrapper, 2, thread->start_routine, thread->arg);
	printf("makecontext() line reached for thread %d\n", thread->tid); // TODO remove
	// Add thread to queue
	TAILQ_INSERT_TAIL(&queue_head, thread, entries);

	// Return 0 on success as per pthread man page
	return 0;
}

void gtthread_exit(void *retval) {
	// Temporary accessor variables
	gtthread_t *thread = TAILQ_FIRST(&queue_head);
	gtthread_t *temp = TAILQ_NEXT(thread, entries);
	
	// Switch context to the next item in the queue
	setcontext(temp->context);

	// Delete the current thread (head) from the queue
	TAILQ_REMOVE(&queue_head, thread, entries);
	free(thread);
	gtthread_count--;
}
