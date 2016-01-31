#include"mypthread.h"


static mypthread_real threadPool[THREAD_COUNT];
static mypthread_t currThread;
static int countThreads = 1;

//some helpers
mypthread_t getUnusedThread() {

	int i;
	mypthread_t currThread = threadPool;
	for(i=0; i < THREAD_COUNT; i++) {
		if((currThread+i)->status == UNUSED) {
			return currThread+i;
		}
	}
	return NULL;
}
mypthread_t getPausedThread() {

	int threadOffset;
	threadOffset = (currThread == 0) ? 0 : currThread-threadPool;
	//printf("t%d\t", threadOffset);

	int i=0;
	mypthread_real *currThread = threadPool;
	for(i=threadOffset;i<countThreads;i++) {
		if((currThread+i)->status == PAUSED) {
			return currThread+i;
		}
	}
	for(i=0;i<threadOffset;i++) {
		if((currThread+i)->status == PAUSED) {
			return currThread+i;
		}
	}
	return NULL; //error, handled by caller
}

struct pthrarg
{
    int *num;
    int size;
};

int mypthread_create(mypthread_t *thread, const mypthread_attr_t *attr, void *(*start_routine) (void *), void *arg) 
{

	if(countThreads == THREAD_COUNT-1) {
		printf("Too many threads\n");
		return -1;
	}

	mypthread_t ret;

	ret = getUnusedThread();
	if(ret == NULL) {
		//This shouldn't ever be reached because of the check above
		printf("Couldn't find UNUSED thread\n");
		return -1;
	}

	countThreads++;

	if(getcontext(&(ret->ctx)) != 0) {
		fprintf(stderr, "Unable to get context\n");
		return -1;
	}
	ret->ctx.uc_stack.ss_sp = ret->stk;
	ret->ctx.uc_stack.ss_size = STACK_SIZE;
	//ret->ctx.ul_link = ;
	
	struct pthrarg *temp;	
	temp = (struct pthrarg *) arg;

	makecontext(&(ret->ctx), (void (*)(void))start_routine, 1, arg);

	ret->status = PAUSED;

	*thread = ret; //This line is important, I think it needs to be fixed

	return 0;
}

/*
 * mypthread_exit has 3 responsibilities
 * 1) sets its own status to UNUSED
 * 2) decrements the thread count
 *
 * 3) If the thread count is 0 (meaning this is the last thread)
 * 		-call exit(3), terminating the program
 * 	otherwise switch to a context of a PAUSED thread
 */

void mypthread_exit(void *retval) {	

	if(!currThread && (countThreads == 1)) {
		//User calls exit before create
		exit(0);
	}

	if(!currThread) {
		//User calls exit before join or yield, but after create
		printf("Need to make this edge case\n");
	}

	currThread->status = UNUSED;
	countThreads--;

	if(countThreads == 0) {
		exit(0);
	}

	if(currThread->parent) {
		setcontext(&(currThread->parent->ctx));
	}

	currThread = getPausedThread();
	if(!currThread) {
		printf("Unable to find paused thread!");
	}

	currThread->status = ACTIVE;

	if(setcontext(&(currThread->ctx)) == -1) {
		printf("Unable to set context!");
	}
}

int mypthread_yield(void) {
	
	mypthread_t thread =getPausedThread();
	if(thread == 0) {
		printf("Error, either by user or yield getPausedThread() failed");
	}

	if(currThread == NULL) {
		//Should only happen on very first attempted join/ yield
		//Means main thread needs to be set up
		mypthread_t mainThread = getUnusedThread();
		mainThread->status = PAUSED;

		thread->status = ACTIVE;
		currThread = thread;

		if(swapcontext(&(mainThread->ctx), &(currThread->ctx)) == -1 ) {
			printf("SwapContext failed\n");
			return -1;
		}

		return 0;
	}


	mypthread_real *oldThread = currThread;

	oldThread->status = PAUSED;
	thread->status = ACTIVE;

	currThread = thread;

	if(setcontext(&(currThread->ctx)) == -1) {
		//panic!
		fprintf(stderr, "Unable to set context!\n");
		return -1;
	}

	return 0;
	
}

int mypthread_join(mypthread_t thread, void **retval) {

	if(thread == 0) {
		//Error, either by user or yield getPausedThread() failed
		fprintf(stderr, "Something bad happened");
		return -1;
	}

	if(thread->status == UNUSED) {
		fprintf(stderr, "Recived uninitialized thread");
		return -1;
	}

	if(currThread == NULL) {
		//Should only happen on very first attempted join/ yield
		//Means main thread needs to be set up
		mypthread_real *mainThread = getUnusedThread();
		mainThread->status = WAITING;
		thread->parent = mainThread;
		thread->status = ACTIVE;
		currThread = thread;

		if(swapcontext(&(mainThread->ctx), &(currThread->ctx)) == -1 ) {
			fprintf(stderr, "SwapContext failed\n");
			return -1;
		}
		return 0;
	}

	mypthread_real *oldThread = currThread;

	oldThread->status = WAITING;
	thread->parent = currThread;
	thread->status = ACTIVE;

	currThread = thread;

	if(swapcontext(&(oldThread->ctx), &(currThread->ctx)) == -1) {
		//panic!
		fprintf(stderr, "Unable to set context!\n");
		return -1;
	}

	return 0;
	
}

