#ifndef SEARCHTHREAD_H
#define SEARCHTHREAD_H

static unsigned long N_THREADS;
static thrd_t *restrict g_threads;
static mtx_t g_mtx;
static cnd_t g_cnd;
static atomic_ulong g_round;
static atomic_bool g_stop;

static inline int SearchThread_main(void *const restrict _thrArg)
{
    (void)(_thrArg);

    unsigned long round = 0;

    for (;;)
    {
        mtx_lock(&g_mtx);

        while (round == atomic_load_explicit(&g_round, memory_order_relaxed))
        {
            cnd_wait(&g_cnd, &g_mtx);
        }

        round = atomic_load_explicit(&g_round, memory_order_relaxed);

        mtx_unlock(&g_mtx);

        if (atomic_load_explicit(&g_stop, memory_order_acquire))
        {
            break;
        }
    }

    return 0;
}

#endif // SEARCHTHREAD_H //