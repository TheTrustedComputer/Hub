/*
 *  Author: 2026- TheTrustedComputer
 *
 *  A centralized collection of shared includes for "Four the Win!".
 */

#ifndef COMMON_H
#define COMMON_H

#ifdef __wasm__
    #define FTW_WEBASM
#endif

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdatomic.h>

#if __has_include(<stdbit.h>)
    #include <stdbit.h>
#else
    #include "../Compat/stdbit.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include(<threads.h>)
    #include <threads.h>
#else
    #include "../Compat/threads.h"
#endif

#include <time.h>

#ifdef FTW_XXHASH
    #include "xxhash.h"
#endif

#ifdef FTW_SQLITE
    #include "sqlite3.h"
#endif

//////////////////////////////////////////////////////////////////////////
/// @brief  C23 monotonic time helper (not yet available on all systems).
/// @param  _ts
//////////////////////////////////////////////////////////////////////////
static inline int FourTheWin_monoTime(struct timespec *const restrict _ts)
{
#ifdef TIME_MONOTONIC
    return timespec_get(_ts, TIME_MONOTONIC);
#elifdef _POSIX_C_SOURCE
    return clock_gettime(CLOCK_MONOTONIC, _ts);
#else
    return timespec_get(_ts, TIME_UTC);
#endif
}

#ifdef __linux__
    #include <sys/sysinfo.h>
    #include <sys/resource.h>
    static constexpr rlim_t FTW_STACK_SIZE = 67108864;
#elif defined(_WIN64) || defined(_WIN32)
    #include <windows.h>
#endif

#define FTW_VOID_NOP (void)(0)
#define FTW_PROFILE_HOT __attribute__((hot))
#define FTW_PROFILE_COLD __attribute__((cold))
#define FTW_BRANCH_HOT(_x) __builtin_expect(_x, true)
#define FTW_BRANCH_COLD(_x) __builtin_expect(_x, false)
#define FTW_BRANCH_PROB(_x, _y) __builtin_expect(_x, _y)

#ifdef FTW_INT_WIDTH
    #undef FTW_LIBDIVIDE
    #undef FTW_FASTMOD
#endif

#ifdef FTW_LIBDIVIDE
    #include "../libdivide/libdivide.h"
#endif

#ifdef FTW_FASTMOD
    #include "../fastmod/include/fastmod.h"
#endif

#include "../Xoshiro.h"
#include "../BumpPool.h"
#include "../Queue.h"

#include "Messages.h"
#include "Connect4.h"
#include "Make7.h"
#include "ResultStat.h"

#ifndef FTW_SQLITE
    #include "ResultRing.h"
#endif

#include "HashRing.h"
#include "TransTable.h"
#include "PathTable.h"
#include "Database.h"
#include "NegaScout.h"
#include "MonteCarlo.h"
#include "PerfTest.h"
#include "Interface.h"

#ifdef FTW_WEBASM
    #include "WebAsmAPI.h"
#endif


#endif // COMMON_H //
