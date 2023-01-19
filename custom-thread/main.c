#include "custom_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>

// opaque handles.
// use them as:
//		_MyThread internal_name = (_MyThread)parameter_name;
// returning these handles as:
//		MyThread parameter_name = (MyThread)internal_name;
typedef void *MyThread;
typedef void *MyBarrier;
typedef void *MyMutex;
typedef void *MyChannel;
typedef struct _MyChannel _MyChannel;
typedef struct _MyThread _MyThread;
typedef struct ThdList ThdList;
typedef struct _MyMutex _MyMutex;
typedef struct _MyBarrier _MyBarrier;

//Structs declaration
struct _MyThread
{
    int id;
    ucontext_t context;
    _MyThread *next;
    _MyThread *prev;
    _MyThread *parent;
    _MyThread *children_thd[1000];
    int num_children;
    int child_spot;
    char blocked;
    _MyThread *join_child;
    char exit;
    int visited;
};

struct _MyMutex
{
    int is_locked;
};

struct _MyBarrier
{
    int counter;
    int is_locked;
    _MyMutex *mutex;
    int count;
};

//linked list
struct ThdList
{
    _MyThread *head;
    _MyThread *tail;
    int value;
};

//prototypes
int threadSchedule();
void enqueue(ThdList *l, _MyThread *t);
_MyThread* dequeue(ThdList *l);

//Thread objects
ThdList ready_queue;
_MyThread *running_thd;
_MyThread *main_thd;
ucontext_t scheduler_context;
ucontext_t init_context;
int id_gen = 1;


// Handles thread operations when exiting and blocking
int threadSchedule(void)
{
    while (1) {
        _MyThread *t;
        if (!ready_queue.head) {
            return 0;
        }
        t = dequeue(&ready_queue);
        // Error handle
        
        running_thd = t;
        swapcontext(&scheduler_context, &running_thd->context);
        
        t = running_thd;
        if (t->exit) {
            // Unblocking is handled in MyThreadExit before swapping here
            free(t->context.uc_stack.ss_sp);
            free(t);
            t = NULL;
        }
    }
}

//Assign funct to Thread
_MyThread* setupThread(void(*start_funct)(void *), void *args)
{
    _MyThread *t = calloc(1, sizeof(_MyThread));
    // Error handle
    if (getcontext(&t->context) == -1) {
        printf("getcontext failed\n");
    }
    t->context.uc_stack.ss_sp = calloc(1, SIGSTKSZ);
    // Error handle

    //Print funct address out of curiosity
    // void   *funptr = &start_funct;
    // backtrace_symbols_fd(&funptr, 1, 1);
    
    t->context.uc_stack.ss_size = SIGSTKSZ;
    //Should it be linked to the scheduler context??
    t->context.uc_link = NULL;
    makecontext(&t->context, (void(*)())start_funct, 1, args);
    
    return t;
}

//Thread Create
MyThread MyThreadCreate(void(*start_funct)(void *), void *args)
{
    _MyThread *t = setupThread(start_funct, args);
    t->id = id_gen++;
    t->parent = running_thd;
    t->num_children = 0;
    t->child_spot = running_thd->num_children;
    t->blocked = 0;
    t->join_child = NULL;
    t->visited = 0;
    running_thd->children_thd[running_thd->num_children++] = t;
    
    // Put thread in ready queue
    enqueue(&ready_queue, t);
    
    return (MyThread) t;
}

//Thread Self
int MyThreadSelf(MyThread thread) {

    _MyThread *t = (_MyThread*) thread;
    return t->id;
}

// Yield invoking thread
void MyThreadYield(void)
{
    if (ready_queue.head) {
        enqueue(&ready_queue, running_thd);
        running_thd = dequeue(&ready_queue);
        swapcontext(&ready_queue.tail->context, &running_thd->context);
    } else {
        // Do nothing and keep running. Nothing else in ready queue.
    }
}

// Join with all children
void MyThreadJoinAll(void)
{
    if (!running_thd->num_children) {
        return;
    }
    // Block the parent
    running_thd->blocked = 1;
    
    swapcontext(&running_thd->context, &scheduler_context);
    // Returned after unblocking
    return;
}

// Terminate invoking thread
void MyThreadExit(void)
{
    // Handle any blocked parent
    _MyThread *p = running_thd->parent;
    if (p) {
        if (p->blocked) {
            if (p->join_child) { 
                if (p->join_child->id == running_thd->id) {
                    p->blocked = 0;
                    p->join_child = NULL;
                    enqueue(&ready_queue, p);
                }
            } else { 
                if (p->num_children == 1) {
                    p->blocked = 0;
                    enqueue(&ready_queue, p);
                }
            }
        }
        // Update children status in the parent thread
        int temp = running_thd->child_spot;
        p->children_thd[temp] = p->children_thd[p->num_children - 1];
        p->children_thd[temp]->child_spot = temp;
        p->num_children--;
    }
    // Null all the parent pointer of its children
    int i;
    for (i = 0; i < running_thd->num_children; i++) {
        running_thd->children_thd[i]->parent = NULL;
    }
    // Set exit tag so the scheduler knows to stop thread
    running_thd->exit = 1;
    swapcontext(&running_thd->context, &scheduler_context);
}


// Create and run the "main" thread
void MyThreadInit(void(*start_funct)(void *), void *args)
{
    // Create the main thread with parameters and put on queue
    main_thd = setupThread(start_funct, args);
    enqueue(&ready_queue, main_thd);
    threadSchedule();
}

//Barrier Init
MyBarrier MyBarrierInit(int threadCount)
{
    _MyBarrier *barrier = calloc(1, sizeof(_MyBarrier));
    barrier->mutex = (MyMutex)MyMutexInit();
    barrier->count = threadCount;
    barrier->counter = threadCount;
    barrier->is_locked = 1;
}


//Barrier Wait
int MyBarrierWait(MyBarrier barrier)
{
    _MyBarrier *bar = (_MyBarrier*) barrier;

    MyMutexLock(bar->mutex);

    int counter = bar->counter;

    if(--counter == 0)
    {
        //Set the counter to the count in order to satisfy the cond
        bar->counter = bar->count;
        bar->is_locked = 0;
        //notifyAll()

        MyMutexUnLock(bar->mutex);
        return 0;
    }
    else
    {
        bar->counter--;
        return bar->counter;
    }
}

//Barrier Destroy
int MyBarrierDestroy(MyBarrier barrier)
{
    _MyBarrier *bar = (_MyBarrier*) barrier;

    MyMutexDestroy(bar->mutex);
    bar->count = 0;
    bar->counter = 0;
    bar->is_locked = 0;
   
    return 0;
}

//Mutex Init
MyMutex MyMutexInit(void)
{
    _MyMutex *mut = calloc(1, sizeof(_MyMutex));
    mut->is_locked=1;

    return (MyMutex) mut;
}

//Mutex Lock
int MyMutexLock(MyMutex mut)
{
    _MyMutex *mutex = (_MyMutex*) mut;

    while (mutex->is_locked)
        MyThreadYield();

    //mutex->is_locked = 1;

    return 0;
}

//Mutex Unlock
int MyMutexUnLock(MyMutex mut)
{
    _MyMutex *mutex = (_MyMutex*) mut;

    //Mutex already unlocked return error
    if(!mutex->is_locked)
        return -1;

    //Dequeue the next thread 
    threadSchedule();

    //Free the lock
    mutex->is_locked = 0;

    return 0;
}

//Mutex Destroy
int MyMutexDestroy(MyMutex mut)
{
    _MyMutex *mutex = (_MyMutex*) mut;

    //Free the lock
    mutex->is_locked = -1;

    return 0;
}

// Add to queue
void enqueue(ThdList *l, _MyThread *t)
{
    if(l->tail) {
        l->tail->next = t;
        t->prev = l->tail;
    } else{
        l->head = t;
        t->prev = NULL;
    }
    l->tail = t;
    t->next = NULL;
}

// Dequeue is based on the delete thread method
// Remove head from queue and return item
_MyThread* dequeue(ThdList *l)
{
    _MyThread* t = NULL;
    if (l->head) {
        t = l->head;
        l->head = t->next;
    }
    if (t->next) {
        t->next->prev = NULL;
        t->next = NULL;
    } else {
        l->tail = NULL;
    }
    return t;
}

// evaluate a Fibonacci number:
//	fib(0) = 0
//	fib(1) = 1
//	fib(n) = fib(n-1) + fib(n-2)  [n>1]
// also, the function parameter is a value/result -- therefore it is a
// pointer to an integer.
//
void fib(void *in)
{
  int *n = (int *)in;	 	/* cast input parameter to an int * */

  if (*n == 0)
    /* pass */;			/* return 0; it already is zero */

  else if (*n == 1)
    /* pass */;			/* return 1; it already is one */

  else {
    int n1 = *n - 1;		/* child 1 param */
    int n2 = *n - 2;		/* child 2 param */

    // create children; p   arameter points to int that is initialized.
    // this is the location they will write to as well.
    MyThreadCreate(fib, (void*)&n1);
    MyThreadCreate(fib, (void*)&n2);
    // after creating children, wait for them to finish

    listAllThreads();
    
    MyThreadJoinAll();
    //  write to addr n_ptr points; return results in addr pointed to
    //  by input parameter
    *n = n1 + n2;
  }

//exit thread
  MyThreadExit();	
}

//THREAD EXTENDED

//just traverses the linked list queue and prins the info about the thread
void listAllThreads() {
    _MyThread *current = ready_queue.head;
    while(current != NULL) {
        printf("Thread ID: %d\n", current->id);
        printf("State: %s\n", current->blocked ? "Blocked" : "Ready");
        printf("Parent Thread ID: %d\n", current->parent->id);
        current = current->next;
    }
}


void detectDeadlocks() {
    _MyThread *current = ready_queue.head;
    while(current != NULL) {
        // Mark the current thread as visited
        current->visited = 1;

        _MyThread *child = current->join_child;
        if(child != NULL && child->visited == 1) {
            printf("Deadlock detected involving thread %d and %d\n", current->id, child->id);
        }
        current = current->next;
    }
}

//handle the signal that checks for the deadlocks
void deadlock_handler(int signum) {
    detectDeadlocks();
}

struct _MyChannel{
 int buffer[10000];
 int head;
 int tail;
 _MyMutex *lock;
};

//channel init
MyChannel MyChannelInit() {
    _MyChannel *channel = (_MyChannel *) malloc(sizeof(_MyChannel));
    channel->lock = (MyMutex)MyMutexInit();
    return (MyChannel) channel;
}

//send the data to the channel
//gets the channel pointer
//sets the data in the tail position of the buffer
//increments the tail position
void MyChannelSend(MyChannel channel, int data) {
    _MyChannel *chan = (_MyChannel *) channel;
    _MyMutex *mutex = (MyMutex)chan->lock;
    MyMutexLock(mutex);
    chan->buffer[chan->tail] = data;
    chan->tail = (chan->tail + 1) % 10000;
    MyMutexUnLock(mutex);
}

//receive the data
//gets the channel pointer
//get the data from the head position
//increment the head position in the channel
//return the data
int MyChannelReceive(MyChannel channel) {
    _MyChannel *chan = (_MyChannel *) channel;
    _MyMutex *mutex = (MyMutex)chan->lock;
    MyMutexLock(mutex);
    int data = chan->buffer[chan->head];
    chan->head = (chan->head + 1) % 10000;
    MyMutexUnLock(mutex);
    return data;
}

//destroy the channel after we are done
void MyChannelDestroy(MyChannel channel) {
    _MyChannel *chan = (_MyChannel *) channel;
    MyMutexDestroy(chan->lock);
    free(chan);
}

int main(int argc, char *argv[])
{
  int n;
  
  n = 6;
  if (n < 0 || n > 10) {
    printf("invalid value for n (%d)\n", n);
    exit(-1);
  }

  printf("fib(%d) = ", n);
  MyThreadInit(fib, (void*)&n);
  signal(SIGUSR1, deadlock_handler);
  printf("%d\n", n);
  return 0;
}
