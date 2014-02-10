#include "gtthread.h"

#define _XOPEN_SOURCE 600 // TODO: remove

#include <stdlib.h>
#include <sys/queue.h>
#include <ucontext.h>


/**********************************
 * A single thread's data structure
 **********************************/


struct gtthread_t {
	// Thread ID
	int tid;
	
	// Function that it points to with its arguments (NULL if none)
	void *(*start_routine)(void *);
	void *arg;

	// Context for this thread, used while switching
	ucontext_t *context;

	// Pointer macro used by TAILQ queue implemenetation
	TAILQ_ENTRY(gtthread_t) entries;
};


/*******************************************
 * Global variables used throughout gtthread
 *******************************************/


// Timeslice for each thread's execution
long gtthread_period = -1;
// Maintain a globajl thread count
int gtthread_count = 0;
// Used for assigning new thread IDs, this never decrements
int gtthread_id = 0;
// Stack size of 8 KB for each context
const int CONTEXT_MAX = 8192;
// Overarching context used by main()
ucontext_t main_context;
// The head of the queue that holds our threads
// @Citation: Usage of TAILQ inspired from http://blog.jasonish.org/2006/08/tailq-example.html
TAILQ_HEAD(, gtthread_t) queue_head;


/***********************************
 * Function definitions for gtthread
 ***********************************/


// This is a debugging function
void gtthread_print_all() {
    struct gtthread_t *thread;
    TAILQ_FOREACH(thread, &queue_head, entries) {
            printf("Thread found with id: %d\n", thread->tid);
    }
}

void gtthread_init(long period) {
	gtthread_period = period;
	TAILQ_INIT(&queue_head);
	printf("gtthread initialized\n"); // TODO: remove
}

int gtthread_create(struct gtthread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg) {
	// Allocate memory for thread
	thread = (struct gtthread_t*)malloc(sizeof(struct gtthread_t));
	
	// Populate it
	thread->start_routine = start_routine;
	thread->arg = &arg;
	thread->tid = gtthread_id++;
	gtthread_count++;

	// Create context for this thread of specified size
	ucontext_t new_context;
	thread->context = &new_context;

	if(getcontext(thread->context) == -1) {
		perror("Error: getcontext() failed to get context!");
	}

	char stack_size[CONTEXT_MAX];
	thread->context->uc_stack.ss_sp = stack_size;
	thread->context->uc_stack.ss_size = sizeof(stack_size);
	
	// Which context should execute once this ends? // TODO
	thread->context->uc_link = &main_context;

	// Initialize the context for this thread
	makecontext(thread->context, thread->start_routine, thread->arg);

	// Add thread to queue
	TAILQ_INSERT_TAIL(&queue_head, thread, entries);

	// Return 0 on success as per pthread man page
	return 0;
}

void gtthread_exit(void *retval) {
	// Temporary accessor variables
	struct gtthread_t *thread = TAILQ_FIRST(&queue_head);
	struct gtthread_t *temp = TAILQ_NEXT(thread, entries);
	
	// Switch context to the next item in the queue
	setcontext(temp->context);

	// Delete the current thread (head) from the queue
	TAILQ_REMOVE(&queue_head, thread, entries);
	free(thread);
}
