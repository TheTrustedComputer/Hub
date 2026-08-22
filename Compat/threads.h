/*
 *  Author: 2020- TheTrustedComputer
 *  
 *  A compatibility layer that wraps C11 thread functions to their POSIX equivalents.
 *  We will only implement a subset of the functions; support for others is not provided.
 */

#ifndef COMPAT_THREADS_H
#define COMPAT_THREADS_H

#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef int (thrd_start_t)(void *const restrict);

enum // Mutex types
{
    mtx_plain = 1,
    mtx_timed = 2,
    mtx_recursive = 4
};

enum // Thread error status
{
    thrd_success,
    thrd_error,
    thrd_nomem,
};

typedef struct // To pass C11 thread arguments
{
    thrd_start_t *func;
    void *restrict arg;
}
thrd_start_wrapper_t;

/////////////////////////////////////////////////////////
/// @brief      POSIX wrapper function for thrd_start_t.
/// @param _ARG Argument to pass to the function.
/// @return     Return value of the function.
/////////////////////////////////////////////////////////
void *thrd_start_wrapper(void *const restrict _ARG)
{
    thrd_start_wrapper_t *wrapper = _ARG;
    int retVal = wrapper->func(wrapper->arg);
    
    free(wrapper);
    
    return (void*)(intptr_t)(retVal);
}

//////////////////////////////////////////////////////////////
/// @brief          Creates a thread executing `_func(_arg)`.
/// @param _thr     Pointer to the new thread.
/// @param _func    Function to execute.
/// @param _arg     Argument to pass to _func().
/// @return         `thrd_success` on allocation success;
///                 `thrd_error` on allocation failure;
///                 `thrd_nomem` on insufficient memory.
/////////////////////////////////////////////////////////////
int thrd_create(thrd_t *const restrict _thr, thrd_start_t _func, void *const restrict _arg)
{
    thrd_start_wrapper_t *wrapper;
    
    if (!(wrapper = malloc(sizeof(*wrapper))))
    {
        return thrd_nomem;
    }
    
    wrapper->func = _func;
    wrapper->arg = _arg;
    
    switch (pthread_create(_thr, NULL, thrd_start_wrapper, wrapper))
    {
    case 0:
        return thrd_success;
    case EAGAIN:
        return thrd_nomem;
    default:
        return thrd_error;
    }
}

/////////////////////////////////////////////////////////////////////////
/// @brief      Blocks the current thread until `_thr` finishes.
/// @param _thr Thread to join.
/// @param _res Output parameter to store the status of `_thr`.
/// @return     `thrd_success` if successful; otherwise `thrd_error`.
/// @note       Undefined if `_thr` has been detached or already joined.
/////////////////////////////////////////////////////////////////////////
int thrd_join(thrd_t _thr, int *const restrict _res)
{
    void *res;
    int joinRes = pthread_join(_thr, &res);
    
    _res && res ? *_res = *(int*)(res) : 0;
    
    return joinRes ? thrd_error : thrd_success;
}

//////////////////////////////////////////////////////////////
/// @brief      Suspends execution of the current thread.
/// @param _DUR Duration to sleep in nanoseconds.
/// @param _rem Output parameter to store the remaining time.
/// @return     Zero if successful; otherwise negative.
//////////////////////////////////////////////////////////////
int thrd_sleep(const struct timespec *const restrict _DUR, struct timespec *const restrict _rem)
{
    return nanosleep(_DUR, _rem);
}

/////////////////////////////////////////////////////////////////////////
/// @brief  Hints the operating system to reschedule the running thread.
/// @note   Behavior is implementation-defined.
/////////////////////////////////////////////////////////////////////////
void thrd_yield(void)
{
    sched_yield();
}

////////////////////////////////////////////////////////////////////////////
/// @brief          Initializes a mutex lock.
/// @param _mtx     Pointer to the mutex.
/// @param _TYPE    Mutex type (`mtx_plain`, `mtx_timed`, `mtx_recursive`).
/// @return         `thrd_success` if successful; otherwise `thrd_error`.
////////////////////////////////////////////////////////////////////////////
int mtx_init(mtx_t *const restrict _mtx, const int _TYPE)
{
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    
    switch (_TYPE)
    {
    case mtx_plain:
        _TYPE & mtx_recursive ? pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) : pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
        break;
    case mtx_timed:
        _TYPE & mtx_recursive ? pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) : pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_TIMED_NP);
        break;
    }
    
    int mtxErrorStatus = pthread_mutex_init(_mtx, &attr);
    
    pthread_mutexattr_destroy(&attr);
    
    return mtxErrorStatus ? thrd_error : thrd_success;
}

////////////////////////////////////////////////////////////
/// @brief      Destroys a mutex lock.
/// @param _mtx Pointer to the mutex.
/// @note       Undefined if threads are waiting on `_mtx`.
////////////////////////////////////////////////////////////
void mtx_destroy(mtx_t *const restrict _mtx)
{
    pthread_mutex_destroy(_mtx);
}

///////////////////////////////////////////////////////////////////////////
/// @brief      Blocks the current thread until the mutex is locked.
/// @param _mtx Pointer to the mutex.
/// @return     `thrd_success` if successful; otherwise `thrd_error`.
/// @note       Undefined if the mutex is already locked or not recursive.
///////////////////////////////////////////////////////////////////////////
int mtx_lock(mtx_t *const restrict _mtx)
{
    return pthread_mutex_lock(_mtx) ? thrd_error : thrd_success;
}

///////////////////////////////////////////////////////////////////////
/// @brief      Unlocks a mutex, unblocking all threads waiting on it.
/// @param _mtx Pointer to the mutex.
/// @return     `thrd_success` if successful; otherwise `thrd_error`.
/// @note       Undefined if the mutex is not previously locked.
///////////////////////////////////////////////////////////////////////
int mtx_unlock(mtx_t *const restrict _mtx)
{
    return pthread_mutex_unlock(_mtx) ? thrd_error : thrd_success;
}

/////////////////////////////////////////////////////////
/// @brief          Initializes a condition variable.
/// @param _cond    Pointer to the condition variable.
/// @return         `thrd_success` if successful;
///                 `thrd_nomem` if insufficient memory;
///                 `thrd_error` if other errors occur.
/////////////////////////////////////////////////////////
int cnd_init(cnd_t *const restrict _cond)
{
    switch (pthread_cond_init(_cond, NULL))
    {
    case 0:
        return thrd_success;
    case ENOMEM:
    case EAGAIN:
        return thrd_nomem;
    default:
        return thrd_error;
    }
}

///////////////////////////////////////////////////////////////////
/// @brief      Destroys a condition variable.
/// @param _cnd Pointer to the condition variable.
/// @note       Undefined if there is a waiting thread on `_cnd`.
///////////////////////////////////////////////////////////////////
void cnd_destroy(cnd_t *const restrict _cnd)
{
    pthread_cond_destroy(_cnd);
}

//////////////////////////////////////////////////////////////////////////
/// @brief      Sends a signal to a waiting thread to continue execution.
/// @param _cnd Pointer to the condition variable.
/// @return     `thrd_success` if successful; otherwise `thrd_error`.
//////////////////////////////////////////////////////////////////////////
int cnd_signal(cnd_t *const restrict _cnd)
{
    return pthread_cond_signal(_cnd) ? thrd_error : thrd_success;
}

//////////////////////////////////////////////////////////////////////
/// @brief      Sends a signal to all waiting threads.
/// @param _cnd Pointer to the condition variable.
/// @return     `thrd_success` if successful; otherwise `thrd_error`.
//////////////////////////////////////////////////////////////////////
int cnd_broadcast(cnd_t *const restrict _cnd)
{
    return pthread_cond_broadcast(_cnd) ? thrd_error : thrd_success;
}

//////////////////////////////////////////////////////////////////////
/// @brief      Atomically unlocks a mutex and waits for a signal.
/// @param _cnd Pointer to the condition variable.
/// @param _mtx Pointer to the mutex.
/// @return     `thrd_success` if successful; otherwise `thrd_error`.
/// @note       Undefined if the mutex is not already locked.
//////////////////////////////////////////////////////////////////////
int cnd_wait(cnd_t *const restrict _cnd, mtx_t *const restrict _mtx)
{
    return pthread_cond_wait(_cnd, _mtx) ? thrd_error : thrd_success;
}

#endif // COMPAT_THREADS_H //
