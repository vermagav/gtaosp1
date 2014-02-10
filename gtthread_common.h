#ifndef __GTTHREAD_COMMON_H
#define __GTTHREAD_COMMON_H

/**********************************************
 * A set of common utilities and variables used
 * across the project.
 **********************************************/

typedef enum { false, true } bool;

void fatal_error(char* message) {
	perror(message);
	exit(0);
}

#endif // __GTTHREAD_COMMON_H