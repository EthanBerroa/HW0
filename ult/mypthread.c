#include "mypthread.h"


static mypthread_real threadPool[THREAD_COUNT];
static mypthread_t currThread;
static int countThreads = 1;

//some helpers
//this function is used for grabbing threads off the THREAD ARRAY "THREADPOOL" initially, when we only have
// one thread thats just been created, might be able to do away with this function
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

//same thing as other function, except its looking for threads with the status of being "PAUSED", again we can prolly combine the two 
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

//This is the only thing I'm still a little unsure about, I think it has to do with how this code interacts with mtsort.c
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
	ret->ctx.uc_stack.ss_sp = ret->stk; //setting base address for an alternate stack for the context ret->ctx, which is a char array inside the ret struct itself
	ret->ctx.uc_stack.ss_size = STACK_SIZE; //setting the stack size, some arbitrary number
	//ret->ctx.ul_link = ; //ul_link defines the context that will be RESUMED at the end of ret->ctx, i guess this is shorthand for "the current context"
	
	struct pthrarg *temp;	
	temp = (struct pthrarg *) arg; //why does he do this? he's storing the argument that's going to be passed into the function that the created thread starts with?

	makecontext(&(ret->ctx), (void (*)(void))start_routine, 1, arg); //i believe this sets the created thread in motion

	ret->status = PAUSED; //why does he pause here?? presumably this is after the created thread has yielded or something, i'm not sure

	*thread = ret; 

	return 0;

	//There is also no reference to currThread in this function, which is curious. One would think that currThread would be set to ret
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
		printf("Need to make this edge case\n"); //hasn't been handled yet, we might want to change this later
	}

	currThread->status = UNUSED;
	countThreads--;

	if(countThreads == 0) {
		exit(0);
	}

	if(currThread->parent) {
		setcontext(&(currThread->parent->ctx)); //return to the context of the threads parent after the thread exits, if it had a parent, traversing up a linked list kinda
	}

	currThread = getPausedThread(); //just grabbing any paused thread off the "stack" i guess
	if(!currThread) {
		printf("Unable to find paused thread!");
	}

	currThread->status = ACTIVE; //resume whatever this thread was doing before it was paused

	if(setcontext(&(currThread->ctx)) == -1) {
		printf("Unable to set context!");
	}
}

int mypthread_yield(void) {
	
	mypthread_t thread = getPausedThread(); //I assume this gets the thread that the current thread is going to yield TO
	if(thread == 0) {
		printf("Error, either by user or yield getPausedThread() failed");
	}

	if(currThread == NULL) {
		//Should only happen on very first attempted join/ yield
		//Means main thread needs to be set up
		mypthread_t mainThread = getUnusedThread(); //this grabs the current calling thread? at least i think
		mainThread->status = PAUSED;

		thread->status = ACTIVE;
		currThread = thread;

		if(swapcontext(&(mainThread->ctx), &(currThread->ctx)) == -1 ) { //begin with whatever the thread yielded to wanted to do
			printf("SwapContext failed\n");
			return -1;
		}
		
		return 0;
	}

	//same stuff as above
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

//THIS IS PROBABLY WHERE THE WAITING TIME IS COMING FROM? NOT 100% SURE YET

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
		mainThread->status = WAITING; //THIS IS THE CALLING THREAD, THE ONE THAT CALLED THE FUNCTION AND WAITS TO WAIT
		thread->parent = mainThread;  
		thread->status = ACTIVE;	//AND THIS IS THE THREAD THAT IS GOING TO BE WAITED FOR
		currThread = thread;

		if(swapcontext(&(mainThread->ctx), &(currThread->ctx)) == -1 ) { //do stuff in the thread we're waiting to join with later
			fprintf(stderr, "SwapContext failed\n");
			return -1;
		}
		return 0;
	}

	//same as above, set calling thread to wait, and swap contexts with the given thread
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


