/*
 *  Author: 2026- TheTrustedComputer
 *
 *  A perft (performance test) is a debugging utility used to benchmark move generation for a given position.
 *  Inspired by similar concepts from chess engines, FTW provides a small perft structure for all supported games.
 */

#ifndef PERFTTEST_H
#define PERFTTEST_H

typedef struct
{
    uint64_t nodes;
    double time;

    union
    {
        struct
        {
            uint64_t drops, pops, passes;
        };

        struct
        {
            uint64_t tile1s, tile2s, tile3s;
        };
    };
}
PerftStat;

//////////////////////////////////////////////////////////////////////////
/// @brief  Writes the results of a perft to standard output (Connect 4).
/// @param  _STAT
//////////////////////////////////////////////////////////////////////////
static inline void PerftStat_Connect4_print(const PerftStat *const restrict _STAT)
{
    printf("%" PRIu64 " node%c\n", _STAT->nodes, _STAT->nodes == 1 ? '\0' : 's');
    printf("%" PRIu64 " drop%c\n", _STAT->drops, _STAT->drops == 1 ? '\0' : 's');

    if (C4_variant == CONNECT4_POPOUT || C4_variant == CONNECT4_POP10)
    {
        printf("%" PRIu64 " pop%c\n", _STAT->pops, _STAT->pops == 1 ? '\0' : 's');

        if (C4_variant != CONNECT4_POPOUT)
        {
            printf("%" PRIu64 " pass%s\n", _STAT->passes, _STAT->passes == 1 ? "" : "es");
        }
    }

    printf("%.9f secs\n", _STAT->time);
    printf("%" PRIu64 " node/sec\n", (uint64_t)(_STAT->nodes / _STAT->time));
}

//////////////////////////////////////////
/// @brief  The perft printer for Make 7.
/// @param  _STAT
//////////////////////////////////////////
static inline void PerftStat_Make7_print(const PerftStat *const restrict _STAT)
{
    printf("%" PRIu64 " node%c\n", _STAT->nodes, _STAT->nodes == 1 ? '\0' : 's');
    printf("%" PRIu64 " #1 tile%c\n", _STAT->tile1s, _STAT->tile1s == 1 ? '\0' : 's');
    printf("%" PRIu64 " #2 tile%c\n", _STAT->tile2s, _STAT->tile2s == 1 ? '\0' : 's');
    printf("%" PRIu64 " #3 tile%c\n", _STAT->tile3s, _STAT->tile3s == 1 ? '\0' : 's');
    printf("%.9f secs\n", _STAT->time);
    printf("%" PRIu64 " node/sec\n", (uint64_t)(_STAT->nodes / _STAT->time));
}

#endif // PERFTTEST_H //
