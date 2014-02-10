#include "gtthread.h"
#include "gtthread_common.h"

//#define _XOPEN_SOURCE 600 // TODO: remove

#include <stdlib.h>
#include <sys/queue.h>
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
const int CONTEXT_MAX = 8192;
// The head of the queue that holds our threads
// @Citation: Usage of TAILQ inspired from http://blog.jasonish.org/2006/08/tailq-example.html
TAILQ_HEAD(, gtthread_t) queue_head;
// Other
const int main_thread_id = -1;
gtthread_t *main_thread;
ucontext_t main_context;
bool initialized = false;


/***********************************
 * Function definitions for gtthread
 ***********************************/


// This is a debugging function
void gtthread_print_all() {
    gtthread_t *thread;
    TAILQ_FOREACH(thread, &queue_head, entries) {
        printf("Thread found with id: %d\n", thread->tid);
        thread->start_routine(thread->arg);
    }
}

static void gtthread_wrapper(void *start_routine(), void *arg)
{
	gtthread_t *thread = TAILQ_FIRST(&queue_head);
	thread->retval = start_routine(arg);	
	// When do we cancel this thread?
	//gtthread_cancel(thread->tid);
	setcontext(&main_context);
	return;
}

void gtthread_init(long period) {
	if(initialized) {
		fatal_error("Error: gtthreads already initialized!");
	}

	// Set round robin cycle period
	gtthread_period = period;
	
	// Initialize queue
	TAILQ_INIT(&queue_head);

	char stack_size[CONTEXT_MAX];
	if (getcontext(&main_context) == 0) {
		main_context.uc_stack.ss_sp = malloc(sizeof(stack_size));
		main_context.uc_stack.ss_size = CONTEXT_MAX;
		main_context.uc_stack.ss_flags = 0;
		main_context.uc_link = NULL;
	}

	// Since gtthread_init() is called only once, create the parent main_thread
	
	// Allocate memory for thread and populate it
	main_thread = (gtthread_t*)malloc(sizeof(gtthread_t));
	getcontext(&main_thread->context);
	main_thread->tid = gtthread_id++;
	gtthread_count++;

	// Create context for this thread of specified size
	main_thread->context = &main_context;
	if(getcontext(&main_thread->context) == -1) {
		fatal_error("Error: getcontext() failed while initializing!");
	}

	main_thread->context->uc_stack.ss_sp = malloc(sizeof(stack_size));
	main_thread->context->uc_stack.ss_size = CONTEXT_MAX;
	main_thread->context->uc_link = &main_context;
	// TODO: should main_context be in thread queue, or is it exempt?

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
	thread->arg = &arg;
	thread->tid = gtthread_id++;
	gtthread_count++;

	printf("Reached point 1 for thread id %d\n", thread->tid);

	// Create context for this thread of specified size
	ucontext_t new_context;
	thread->context = &new_context;

	printf("Reached point 2 for thread id %d\n", thread->tid);

	if(getcontext(&thread->context) == -1) {
		fatal_error("Error: getcontext() failed while creating thread!");
	}

	printf("Reached point 3 for thread id %d\n", thread->tid);

	char stack_size[CONTEXT_MAX];
	thread->context->uc_stack.ss_sp = malloc(sizeof(stack_size));
	thread->context->uc_stack.ss_size = CONTEXT_MAX;
	thread->context->uc_stack.ss_flags = 0;
	thread->context->uc_link = &main_context;

	printf("Reached point 4 for thread id %d\n", thread->tid);

	// Initialize the context for this thread
	makecontext(thread->context, (void(*)(void)) gtthread_wrapper, 2, start_routine, arg);
	
	printf("Reached point 5 for thread id %d\n", thread->tid);

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
