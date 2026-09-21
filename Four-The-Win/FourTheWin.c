/*
 *  Author: 2026- TheTrustedComputer
 *
 *  The entry point for "Four the Win!", our specialized Connect 4 solver.
 */

#ifdef FTW_DEBUG
    //#define FTW_TT_128_BITS
    //#define FTW_XXHASH
    //#define FTW_SQLITE
#endif

#include "Common.h"

int main(void)
{
/*#ifdef __unix__
    {
        struct rlimit rl;

        if (!getrlimit(RLIMIT_STACK, &rl))
        {
            if (rl.rlim_cur < FTW_STACK_SIZE) // 64 MB
            {
                rl.rlim_cur = FTW_STACK_SIZE;

                if (setrlimit(RLIMIT_STACK, &rl) == -1)
                {
                    fprintf(stderr, "\e[1m%s: Could not raise the stack size; certain operations may cause a stack overflow on some systems.\e[0m\n", FTW_STR_ERROR_PREFIX);
                }
            }
        }
    }
#endif*/

#ifdef __unix__
    N_THREADS = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_WIN64) || defined(_WIN32)
    N_THREADS = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
    N_THREADS = 1;
#endif

    REC_mtx_init(&g_mtx, mtx_plain, "Could not initialize the global mutex.", true);
    REC_cnd_init(&g_cnd, "Could not initialize the global condition variable.", true);

    atomic_init(&g_round, 0);
    atomic_init(&g_stop, false);

    g_threads = REC_calloc(N_THREADS, sizeof(*g_threads), "Could not allocate memory for the search threads.", true);

    for (unsigned long i = 0; i < N_THREADS; i++)
    {
        REC_thrd_create(&g_threads[i], SearchThread_main, nullptr, "Could not create the search threads.", true);
    }

    printf("%s by %s\n", FTW_STR_BINARY_NAME, FTW_STR_PROGRAMMER);

    C4_variant = CONNECT4_ORIGINAL;

#ifdef FTW_PGO
    NegaScout_Connect4_PGO();
    NegaScout_Make7_PGO();
#else
    while (UI_run)
    {
        Interface_run();
    }
#endif

    atomic_fetch_add_explicit(&g_round, 1, memory_order_relaxed);
    atomic_store_explicit(&g_stop, true, memory_order_release);
    cnd_broadcast(&g_cnd);

    for (unsigned long i = 0; i < N_THREADS; i++)
    {
        REC_thrd_join(g_threads[i], nullptr, "Could not join the search threads.", true);
    }

    REC_free(g_threads);
    mtx_destroy(&g_mtx);
    cnd_destroy(&g_cnd);

    return EXIT_SUCCESS;
}
