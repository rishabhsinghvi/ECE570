#include<vector>
#include<queue>
#include<map>
#include<cassert>
#include<iostream>
#include<cstdlib>

#include<ucontext.h>

#include "thread.h"
#include "interrupt.h"

struct Thread
{
    unsigned int m_ID;
    ucontext_t* m_Context;
    char* stack;
    bool m_Finished;
};  

struct Lock
{
    Thread* m_Owner;
    std::queue<Thread*> m_WaitQueue;
};

std::queue<Thread*> readyQueue;
ucontext_t* defaultContext = NULL;
Thread* currentRunningThread = NULL;

std::map<unsigned int, std::queue<Thread*> > conditions;
std::map<unsigned int, Lock*> locks;


bool libraryInitialized = false;

static int threadFunc(thread_startfunc_t func, void* arg)
{
#ifdef __DEBUG
    std::cout << "Entering threadFunc...\n";
#endif
    assert_interrupts_disabled();
    interrupt_enable();
    func(arg);
    assert_interrupts_enabled();
    interrupt_disable();

    currentRunningThread->m_Finished = true;

    
#ifdef __DEBUG
    std::cout << "Exiting threadFunc...\n";
#endif


    swapcontext(currentRunningThread->m_Context, defaultContext);
    return 0;
}

int thread_create(thread_startfunc_t func, void* arg)
{
#ifdef __DEBUG
    std::cout << "Entering thread_create...\n";
#endif

    static int threadID = 0;
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();
    Thread* t;
    try
    {
        t = new Thread;
        t->m_Context = new ucontext_t;
        getcontext(t->m_Context);

        t->m_ID = threadID;
        threadID++;
        
        t->stack = new char[STACK_SIZE];
        t->m_Finished = false;
        t->m_Context->uc_stack.ss_sp = t->stack;
        t->m_Context->uc_stack.ss_size = STACK_SIZE;
        t->m_Context->uc_stack.ss_flags = 0;
        t->m_Context->uc_link = NULL;
        makecontext(t->m_Context, (void (*)())threadFunc, 2, func, arg);
        
        readyQueue.push(t);
    }
    catch(std::bad_alloc exc)
    {
        delete t->m_Context;
        delete t->stack;
        delete t;
        assert_interrupts_disabled();
        interrupt_enable();
#ifdef __DEBUG
    std::cout << "Exiting thread_create bad alloc...\n";
#endif
        return -1;
    }
    assert_interrupts_disabled();
    interrupt_enable();

#ifdef __DEBUG
    std::cout << "Exiting thread_create...\n";
#endif

    return 0;
}



static int _thread_unlock(unsigned int lock)
{
    std::map<unsigned int, Lock*>::iterator it = locks.find(lock);

    // if lock doesn't exists, error
    if(it == locks.end())
    {
        //interrupt_enable();
        return -1;
    }

    // if lock exists, but no thread owns it,  error
    Lock* _lock = it->second;

    if(_lock->m_Owner == NULL)
    {
        //assert_interrupts_disabled();
        //interrupt_enable();
        return -1;
    }

    // if lock exists, but current thread is not the owner, error
    if(_lock->m_Owner->m_ID != currentRunningThread->m_ID)
    {
        //assert_interrupts_disabled();
        //interrupt_enable();
        return -1;
    }

    // if lock is owned by current thread
    
    // if any other thread waiting for the lock
    if(!_lock->m_WaitQueue.empty())
    {
        _lock->m_Owner = _lock->m_WaitQueue.front();
        _lock->m_WaitQueue.pop();
        readyQueue.push(_lock->m_Owner);
    }

    else
    {
        _lock->m_Owner = NULL;
    }

    return 0;
}


int thread_signal(unsigned int lock, unsigned int cond)
{
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();

    // find all threads waiting for that cond
    std::map<unsigned int, std::queue<Thread*> >::iterator it = conditions.find(cond);

    // No thread waiting
    if(it == conditions.end())
    {
        
        assert_interrupts_disabled();
        interrupt_enable();
        return 0;
    }


    // If threads are waiting, the pick the first thread and push it onto the ready queue

    if(!it->second.empty())
    {
        Thread* thr = it->second.front();
        it->second.pop();
        readyQueue.push(thr);
    }

    assert_interrupts_disabled();
    interrupt_enable();
    return 0;

}


int thread_broadcast(unsigned int lock, unsigned int cond)
{
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();

    // find all threads waiting for that cond
    std::map<unsigned int, std::queue<Thread*> >::iterator it = conditions.find(cond);

    // No thread waiting
    if(it == conditions.end())
    {
        return 0;
    }


    // If threads are waiting, the pick the first thread and push it onto the ready queue

    // Push all waiting threads to ready queue
    while(!it->second.empty())
    {
        Thread* thread = it->second.front();
        it->second.pop();
        readyQueue.push(thread);
    }

    assert_interrupts_disabled();
    interrupt_enable();
    return 0;

}

int thread_lock(unsigned int lock)
{
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();

    std::map<unsigned int, Lock*>::iterator it = locks.find(lock);

    // insert new lock
    if(it == locks.end())
    {
        Lock* _lock;
        try
        {
         _lock = new Lock;
        }
        catch(std::bad_alloc excp)
        {
            delete _lock;
            assert_interrupts_disabled();
            interrupt_enable();
            return -1;
        }
        _lock->m_Owner = currentRunningThread;
        locks[lock] = _lock;
    }
    else
    {
        Lock* _lock = it->second;
        
        // if lock is free, acquire lock
        if(_lock->m_Owner == NULL)
        {
            _lock->m_Owner = currentRunningThread;
        }
        // if lock is already owned by calling thread,
        // error
        else if(_lock->m_Owner->m_ID == currentRunningThread->m_ID)
        {
            assert_interrupts_disabled();
            interrupt_enable();
            return -1;
        }
        // if lock is owned by some other thread,
        // insert into wait queue
        else
        {
            _lock->m_WaitQueue.push(currentRunningThread);
            swapcontext(currentRunningThread->m_Context, defaultContext);
        }
    }

    assert_interrupts_disabled();
    interrupt_enable();
    return 0;

}


int thread_unlock(unsigned int lock)
{
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();

    int err = _thread_unlock(lock);

    assert_interrupts_disabled();
    interrupt_enable();
    return err;
}


int thread_wait(unsigned int lock, unsigned int cond)
{
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();

    int err = _thread_unlock(lock);

    if(err == 0)
    {
        //std::map<unsigned int, std::queue<Thread*> >::const_iterator it = conditions.find(cond);

        // push thread onto condition waiting queue
        conditions[cond].push(currentRunningThread);
        swapcontext(currentRunningThread->m_Context, defaultContext);
        
        assert_interrupts_disabled();
        interrupt_enable();

        int err = thread_lock(lock);
        return err;
    }

    assert_interrupts_disabled();
    interrupt_enable();
    return -1;
}

int thread_yield()
{
    if(!libraryInitialized)
    {
        return -1;
    }

    assert_interrupts_enabled();
    interrupt_disable();

    readyQueue.push(currentRunningThread);
    swapcontext(currentRunningThread->m_Context, defaultContext);
    
    assert_interrupts_disabled();
    interrupt_enable();
    return 0;
}


int thread_libinit(thread_startfunc_t func, void* args)
{
    // If library is already initialized, call to thread_libinit again will throw an error
    if(libraryInitialized)
    {
        return -1;
    }

    libraryInitialized = true;


    // Create the thread to run the first routine
    if(thread_create(func, args) == -1)
        return -1;
    
    // if queue is empty, we don't have any threads; return error
    if(readyQueue.empty())
    {
        return -1;
    }

    // create the default context for the scheduler
    defaultContext = new ucontext_t; 
    getcontext(defaultContext);

    currentRunningThread = readyQueue.front();
    readyQueue.pop();

    assert_interrupts_enabled();
    interrupt_disable();

    // run the very first thread
    swapcontext(defaultContext, currentRunningThread->m_Context);
    
    // keep scheduling until ready queue is empty
    while(!readyQueue.empty())
    {   
        if(currentRunningThread->m_Finished)
        {
            delete[] currentRunningThread->stack;
            currentRunningThread->m_Context->uc_stack.ss_sp = NULL;
            currentRunningThread->m_Context->uc_stack.ss_size = 0;
            currentRunningThread->m_Context->uc_stack.ss_flags = 0;
            currentRunningThread->m_Context->uc_link = NULL;
            delete currentRunningThread->m_Context;
            delete currentRunningThread;
            currentRunningThread = NULL;
        }

        Thread* thr = readyQueue.front();
        readyQueue.pop();
        currentRunningThread = thr;
        swapcontext(defaultContext, currentRunningThread->m_Context);
    }


    if(currentRunningThread != NULL)
    {
        delete[] currentRunningThread->stack;
        currentRunningThread->m_Context->uc_stack.ss_sp = NULL;
        currentRunningThread->m_Context->uc_stack.ss_size = 0;
        currentRunningThread->m_Context->uc_stack.ss_flags = 0;
        currentRunningThread->m_Context->uc_link = NULL;
        delete currentRunningThread->m_Context;
        delete currentRunningThread;
        currentRunningThread = NULL;
    }

    std::cout << "Thread library exiting.\n";
    exit(0);
}



