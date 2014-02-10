#include "gtthread.h"

/* Initialize gtthread */
void gtthread_init(long period) {
	gtthread_period = period;
	TAILQ_INIT(&queue_head);
	printf("gtthread initialized\n"); // TODO: remove
}

/* Spawn a new thread and add it to the queue */
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

	if(getcontext(thread->context) == -1) {
		perror("Error: getcontext() failed to get context!");
	}

	// Create context for this thread of specified size
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
