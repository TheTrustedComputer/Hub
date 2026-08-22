/*
 *  Author: 2026- TheTrustedComputer
 *
 *  The entry point for "Four the Win!", our specialized Connect 4 solver.
 */

#include "Common.h"

int main(void)
{
#ifdef __linux__
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
#endif

    printf("%s by %s\n", FTW_STR_BINARY_NAME, FTW_STR_PROGRAMMER);

    C4_variant = CONNECT4_ORIGINAL;

#ifdef FTW_PGO
    NegaScout_Connect4_PGO();
    NegaScout_Make7_PGO();
#else
#ifdef FTW_DEBUG
    C4_variant = CONNECT4_MAKE7;
#endif
    while (UI_run)
    {
        Interface_run();
    }
#endif

    return 0;
}
