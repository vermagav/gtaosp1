#ifndef __GTTHREAD_H
#define __GTTHREAD_H

#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <sys/queue.h>
#include <ucontext.h>

/* Timeslice for each thread's execution */
long gtthread_period = -1;
/* Maintain a globajl thread count */
int gtthread_count = 0;
/* Used for assigning new thread IDs, this never decrements */
int gtthread_id = 0;
/* Stack size for each context */
const int CONTEXT_MAX = 8192; // 8 KB
/* Overarching context used by main() */
ucontext_t main_context;
/* The head of the queue that holds our threads */
// @Citation: Usage of TAILQ inspired from http://blog.jasonish.org/2006/08/tailq-example.html
TAILQ_HEAD(, gtthread_t) queue_head;



/* A single thread's data structure */
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



/* Must be called before any of the below functions. Failure to do so may
 * result in undefined behavior. 'period' is the scheduling quantum (interval)
 * in microseconds (i.e., 1/1000000 sec.). */
void gtthread_init(long period);

/* see man pthread_create(3); the attr parameter is omitted, and this should
 * behave as if attr was NULL (i.e., default attributes) */
int  gtthread_create(struct gtthread_t *thread,
                     void *(*start_routine)(void *),
                     void *arg);

/* see man pthread_join(3) */
int  gtthread_join(struct gtthread_t thread, void **status);

/* gtthread_detach() does not need to be implemented; all threads should be
 * joinable */

/* see man pthread_exit(3) */
void gtthread_exit(void *retval);

/* see man sched_yield(2) */
int gtthread_yield(void);

/* see man pthread_equal(3) */
int  gtthread_equal(struct gtthread_t t1, struct gtthread_t t2);

/* see man pthread_cancel(3); but deferred cancelation does not need to be
 * implemented; all threads are canceled immediately */
int  gtthread_cancel(struct gtthread_t thread);

/* see man pthread_self(3) */
struct gtthread_t gtthread_self(void);


/* see man pthread_mutex(3); except init does not have the mutexattr parameter,
 * and should behave as if mutexattr is NULL (i.e., default attributes); also,
 * static initializers do not need to be implemented */
// int  gtthread_mutex_init(gtthread_mutex_t *mutex);
// int  gtthread_mutex_lock(gtthread_mutex_t *mutex);
// int  gtthread_mutex_unlock(gtthread_mutex_t *mutex);

/* gtthread_mutex_destroy() and gtthread_mutex_trylock() do not need to be
 * implemented */

#endif // __GTTHREAD_H
