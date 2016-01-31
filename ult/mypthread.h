#ifndef H_MYPTHREAD
#define H_MYPTHREAD

#define STACK_SIZE 4096
#define THREAD_COUNT 512

#include<stdio.h>
#include<stdlib.h>
#include<ucontext.h>

//Status Enum
typedef enum {
	UNUSED,		//Yet to be allocated, or already finished
	ACTIVE,		//In use, should only be one at a time
	PAUSED,		//Has yielded
	WAITING		//Waiting to be returned from a join
} mypthread_status;

// Types
typedef struct {
	//No need for this, leaving it in just in case
} mypthread_attr_t;

struct mypthread_struct{
	mypthread_status status;
	mypthread_attr_t attr;				//Not used
	ucontext_t ctx;						
	char stk[STACK_SIZE];
	struct mypthread_struct *parent;	//Used for returning from joins
};

typedef struct mypthread_struct mypthread_real;

typedef mypthread_real* mypthread_t;	//This is neccessary because join gives a type not a mypthread_t instead of a pointer to one
										//If not for this, it'd be neccessary to send the entire stack each time with how I'm currently
										//doing things

// Functions
int mypthread_create(mypthread_t *thread, const mypthread_attr_t *attr,
			void *(*start_routine) (void *), void *arg);

void mypthread_exit(void *retval);

int mypthread_yield(void);

int mypthread_join(mypthread_t thread, void **retval);


/* Don't touch anything after this line.
 *
 * This is included just to make the mtsort.c program compatible
 * with both your ULT implementation as well as the system pthreads
 * implementation. The key idea is that mutexes are essentially
 * useless in a cooperative implementation, but are necessary in
 * a preemptive implementation.
 */

typedef int mypthread_mutex_t;
typedef int mypthread_mutexattr_t;


static inline int mypthread_mutex_init(mypthread_mutex_t *mutex,
			const mypthread_mutexattr_t *attr) { return 0; }

static inline int mypthread_mutex_destroy(mypthread_mutex_t *mutex) { return 0; }

static inline int mypthread_mutex_lock(mypthread_mutex_t *mutex) { return 0; }

static inline int mypthread_mutex_trylock(mypthread_mutex_t *mutex) { return 0; }

static inline int mypthread_mutex_unlock(mypthread_mutex_t *mutex) { return 0; }
 
#endif