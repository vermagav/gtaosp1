#include "gtthread.h"
#include "gtthread_common.h"

//#define _XOPEN_SOURCE 600 // TODO: remove

#include <signal.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/time.h> 
#include <ucontext.h>


/***********************************
 * A single thread's data structures
 ***********************************/


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

typedef struct gtthread_mutex_t {
	int parent_tid;
	long lock;
} gtthread_mutex_t;



/*******************************************
 * Global variables used throughout gtthread
 *******************************************/


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
void gtthread_wrapper(void *start_routine(), void *arg) {
	printf("test\n"); // TODO remove
	gtthread_t *thread = TAILQ_FIRST(&queue_head);
	printf("Reached inside wrapper function for thread %d\n", thread->tid); // TODO REMOVE
	thread->retval = start_routine(arg);
	printf("Wrapper start routine called!\n"); // TODO remove
	thread->deleted = true;
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

	// Declare variables for signal timer usage
	// @Citation: Snippet taken from http://www.makelinux.net/alp/069
	struct sigaction sa;
	struct itimerval timer;
	
	// Initialize queue
	TAILQ_INIT(&queue_head);

	// Allocate memory for a new main thread and populate it
	gtthread_t *temp_thread = (gtthread_t*)malloc(sizeof(gtthread_t));
	temp_thread->tid = gtthread_id++;
	temp_thread->deleted = false;
	gtthread_count++;

	// Create context for this thread of specified size
	ucontext_t new_context;
	temp_thread->context = &new_context;
	temp_thread->context->uc_stack.ss_sp = malloc(CONTEXT_STACK_SIZE);
	temp_thread->context->uc_stack.ss_size = CONTEXT_STACK_SIZE;
	temp_thread->context->uc_stack.ss_flags = 0;
	temp_thread->context->uc_link = NULL;
	if(getcontext(temp_thread->context) == -1) {
		fatal_error("Error: getcontext() failed while initializing!");
	}

	// Insert main thread to queue, main thread has id 0
	TAILQ_INSERT_TAIL(&queue_head, temp_thread, entries);

	// Install the gtthread_next() function for the signal as its action
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = gtthread_next;
	sigaction(SIGVTALRM, &sa, NULL);
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = period;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = period;
	setitimer(ITIMER_VIRTUAL, &timer, NULL);
	
	// All done!
	initialized = true;
	printf("gtthread initialized!\n"); // TODO: remove
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
	thread->context->uc_stack.ss_sp = malloc(CONTEXT_STACK_SIZE);
	thread->context->uc_stack.ss_size = CONTEXT_STACK_SIZE;
	thread->context->uc_stack.ss_flags = 0;
	thread->context->uc_link = main_context;
	if(getcontext(thread->context) == -1) {
		fatal_error("Error: getcontext() failed while creating thread!");
	}

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

int gtthread_yield(void) {
	gtthread_next();
}

int gtthread_equal(gtthread_t t1, gtthread_t t2) {
	if(t1.tid == t2.tid) {
		return 1;
	} else {
		return 0;
	}
}

int gtthread_mutex_init(gtthread_mutex_t *mutex) {
	if(mutex->lock == 1) {
		return -1;
	}
	// Set lock to 0 and parent to -1 as per man page
	mutex->lock = 0;
	mutex->parent_tid = -1;
	return 0;
}

int gtthread_mutex_lock(gtthread_mutex_t *mutex) {
	gtthread_t *thread = TAILQ_FIRST(&queue_head);
	if(mutex->parent_tid == thread->tid) {
		return -1;
	}
	
	// Spinlock, keep yielding till lock is acquired with correct parent
	while(mutex->lock !=0 && mutex->parent_tid != thread->tid) {
		gtthread_yield();
	}
	
	// Houston, we have a lock
	mutex->lock = 1;
	mutex->parent_tid = thread->tid;
	return 0;
}

int gtthread_mutex_unlock(gtthread_mutex_t *mutex) {
	gtthread_t *thread = TAILQ_FIRST(&queue_head);
	if(mutex->lock == 1 && mutex->parent_tid == thread->tid) {
		// Set lock to 0 and parent to -1 as per man page
		mutex->lock = 0;
		mutex->parent_tid = -1;
		return 0;
	} else {
		return -1;
	}
}
