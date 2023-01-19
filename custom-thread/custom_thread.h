
    #ifndef MYTHREAD_H
    #define MYTHREAD_H

    typedef void *MyThread;
    typedef void *MyMutex;
    typedef void *MyBarrier;
    typedef void *MyChannel;

    // --------THREAD-------

    // Create a new thread.
    MyThread MyThreadCreate(void(*start_funct)(void *), void *args);

    // Yield invoking thread
    void MyThreadYield(void);

    // Join with
    void MyThreadJoinAll(void);

    // Terminate invoking thread
    void MyThreadExit(void);

    //Init
    void MyThreadInit(void(*start_funct)(void *), void *args);

    //Thread Self
    int MyThreadSelf(MyThread thread);

    // ---------MUTEX----------

    //Mutex Init
    MyMutex MyMutexInit(void);

    //Mutex Lock
    int MyMutexLock(MyMutex mut);

    //Mutex Unlock
    int MyMutexUnLock(MyMutex mut);

    //Mutex Destroy
    int MyMutexDestroy(MyMutex mut);


    // --------BARRIER----------

    //Init
    MyBarrier MyBarrierInit(int threadCount);

    //Barrier Wait
    int MyBarrierWait(MyBarrier barrier);

    //Barrier Destroy
    int MyBarrierDestroy(MyBarrier barrier);

    // --------EXTENDED----------

    //List all threads from the queue
    void listAllThreads();

    //Detect deadlock
    void detectDeadlocks();

    //Handle the signal that checks for the deadlocks
    void deadlock_handler(int signum);

    //Init Channel
    MyChannel MyChannelInit();

    //Send the data from the Channel
    void MyChannelSend(MyChannel channel, int data);

    //Receive data from the Channel
    int MyChannelReceive(MyChannel channel);

    //Destroy the Channel
    void MyChannelDestroy(MyChannel channel);

    #endif