/*
 *  Author: 2026- TheTrustedComputer
 *
 *  NegaScout (PVS) is an optimized game tree search algorithm based on minimax with alpha-beta pruning.
 *
 *  Minimax aims to minimize the worst-case scenario for a player and maximize their own advantage.
 *  However, it is inefficient and may explore branches that are less favorable than the current best.
 *  Alpha-beta pruning addresses this issue by setting lower and upper bounds to prevent such branches.
 *  It it possible for minimax to approach the same position differently in a process called transpositions.
 *  We utilize a hash map to cache transpositions and their scores to avoid reevaluating visited states.
 *
 *  Once we realize one's win is the other's loss, a hallmark of zero-sum games, we can refactor it to negamax.
 *  Negamax ensures the equation MIN(alpha, beta) = -MAX(-beta, -alpha) always holds for these types of games.
 *  We can improve pruning by ordering moves and tightening bounds on suboptimal paths--exactly what NegaScout does.
 *  To enhance efficiency, we leverage the history heuristic to reattempt successful moves that yielded a cutoff.
 */

#ifndef NEGASCOUT_H
#define NEGASCOUT_H

static constexpr int16_t NS_WIN_VAL = 30000;
static constexpr int16_t NS_WIN_THR = NS_WIN_VAL - 10000;
static constexpr int16_t NS_PROG = 1;
static constexpr int16_t NS_DRAW = 0;
static constexpr uint16_t NS_MAX_DEPTH = INT16_MAX;

static TransTable NS_table;
//static PathStack NS_stack;
//static PathGraph NS_graph;
static PathTable NS_path;
static uint64_t *restrict NS_hist;
static struct timespec NS_str, NS_end;
static double NS_time;
static uint64_t NS_nodes;

static int16_t (*NegaScout_Connect4_search)(Connect4 *const restrict, const uint16_t, int16_t, const int16_t);
static Result (*NegaScout_Connect4_iterative)(Connect4 *const restrict, const bool);
static char (*NegaScout_Connect4_results)(Connect4 *const restrict, const bool);

/////////////////////////////////////////////////////
/// @brief  Allocates the history [heuristic] table.
/////////////////////////////////////////////////////
static inline void NegaScout_History_init(void)
{
    NS_hist = REC_calloc(MOVE_SPACE, sizeof(*NS_hist), "Could not allocate memory for the history heuristic table.", true);
}

//////////////////////////////////////////
/// @brief Deallocates the history table.
//////////////////////////////////////////
static inline void NegaScout_History_destroy(void)
{
    REC_free(NS_hist);
}

////////////////////////////////////////////////////////
/// @brief Randomizes all entries in the history table.
////////////////////////////////////////////////////////
static inline void NegaScout_History_randomize(void)
{
    for (uint8_t i = 0; i < MOVE_SPACE; i++)
    {
        NS_hist[i] = SplitMix64_finalize(Xoshiro128pp_next(&g_rng));
    }
}

////////////////////////////////////////////////////////////////
/// @brief  Decay all entries in the history table by one-half.
////////////////////////////////////////////////////////////////
static inline void NegaScout_History_decay(void)
{
    for (uint8_t i = 0; i < MOVE_SPACE; i++)
    {
        NS_hist[i] >>= 4;
    }
}

//////////////////////////////////////////////////////////
/// @brief  Insertion sorts a move list by history score.
/// @param  _arr
/// @param  _STR
/// @param  _OFF
//////////////////////////////////////////////////////////
static inline void NegaScout_History_sort(uint8_t _arr[const restrict], const uint8_t _STR, const uint8_t _OFF)
{
    const uint8_t END = _STR + _OFF;

    for (uint8_t i = _STR + 1; i < END; i++)
    {
        const uint8_t KEY_MOV = _arr[i];
        const uint64_t KEY_SCR = NS_hist[KEY_MOV];

        uint8_t j;

        for (j = i; j > _STR; j--)
        {
            const uint8_t PREV_MOV = _arr[j - 1];

            if (NS_hist[PREV_MOV] >= KEY_SCR)
            {
                break;
            }

            _arr[j] = PREV_MOV;
        }

        _arr[j] = KEY_MOV;
    }
}

//////////////////////////////////////////////////////////////////
/// @brief  Promotes a move to the first entry in the move array.
/// @param  _arr
/// @param  _CNT
/// @param  _MOV
//////////////////////////////////////////////////////////////////
static inline void NegaScout_promote(uint8_t _arr[const restrict static MOVE_BOUNDS], const uint8_t _CNT, const uint8_t _MOV)
{
    for (uint8_t i = 0; i < _CNT; i++)
    {
        if (_arr[i] == _MOV)
        {
            const uint8_t M_SWP = _arr[0];

            _arr[0] = _MOV;
            _arr[i] = M_SWP;

            return;
        }
    }
}

/////////////////////////////////////////////////////////////
/// @brief  Adjusts a terminal value for win/loss distances.
/// @param  _VAL
/////////////////////////////////////////////////////////////
static inline int16_t NegaScout_adjustWinLoss(const int16_t _VAL)
{
    return _VAL >= NS_WIN_THR ? _VAL - 1 : _VAL <= -NS_WIN_THR ? _VAL + 1 : _VAL;
}

//////////////////////////////////////////////////////////
/// @brief      Performs a NegaScout search on Connect 4.
/// @param _c4  Unaliased pointer to the game structure.
/// @param _D   Remaining search depth.
/// @param _a   Alpha/lower bound.
/// @param _B   Beta/upper bound.
/// @return     Minimax value.
//////////////////////////////////////////////////////////
static inline int16_t NegaScout_Connect4_original_search(Connect4 *const restrict _c4, const uint16_t _D, int16_t _a, const int16_t _B)
{
    NS_nodes++;

    if (Connect4_canWin(_c4))
    {
        return NS_WIN_VAL;
    }

    if (!_D)
    {
        return NS_DRAW;
    }

    const TTLock LOCK = Connect4_lock(_c4); int16_t ttVal; uint8_t ttMov;

    TransBucket *const restrict tb = &NS_table.bucket[TransTable_index(&NS_table, LOCK)];

    if (TransBucket_probe(tb, LOCK, _D, _a, _B, &ttVal, &ttMov) == TT_CUT)
    {
        return ttVal;
    }

    int16_t curr, best = -NS_WIN_VAL;
    uint8_t movArr[MOVE_BOUNDS], movCnt, move = UINT8_MAX;

    Connect4_genNonLosing(_c4, movArr, &movCnt);
    ttMov ? NegaScout_promote(movArr, movCnt, ~ttMov), NegaScout_History_sort(movArr, 1, movCnt - 1) : FTW_VOID_NOP;

    const int16_t OLD_A = _a;

    for (uint8_t i = 0; i < movCnt; i++)
    {
        const uint8_t MOV = movArr[i];

        Connect4_play(_c4, MOV);

        i ? (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_original_search(_c4, _D - 1, -_a - 1, -_a)),
             curr = curr > _a && curr < _B ? NegaScout_adjustWinLoss(-NegaScout_Connect4_original_search(_c4, _D - 1, -_B, -_a)) : curr)
          : (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_original_search(_c4, _D - 1, -_B, -_a)));

        Connect4_unplay(_c4);

        if (curr > best)
        {
            best = curr;
            move = MOV;

            if (best > _a)
            {
                _a = best;

                if (_a >= _B)
                {
                    NS_hist[move] += _D * _D;

                    break;
                }
            }
        }
    }

    TransBucket_store(tb, LOCK, best, _D, OLD_A, _B, move);

    return best;
}

//////////////////////////////////////////////////////
/// @brief  The NegaScout driver on Misere Connect 4.
/// @param  _c4
/// @param  _D
/// @param  _a
/// @param  _B
//////////////////////////////////////////////////////
static inline int16_t NegaScout_Connect4_misere_search(Connect4 *const restrict _c4, const uint16_t _D, int16_t _a, const int16_t _B)
{
    NS_nodes++;

    if (Connect4_canWin(_c4))
    {
        return -NS_WIN_VAL;
    }

    if (!_D)
    {
        return NS_DRAW;
    }

    const TTLock LOCK = Connect4_lock(_c4); int16_t ttVal; uint8_t ttMov;

    TransBucket *const restrict tb = &NS_table.bucket[TransTable_index(&NS_table, LOCK)];

    if (TransBucket_probe(tb, LOCK, _D, _a, _B, &ttVal, &ttMov) == TT_CUT)
    {
        return ttVal;
    }

    int16_t curr, best = -NS_WIN_VAL;
    uint8_t movArr[MOVE_BOUNDS], movCnt, move = UINT8_MAX;

    Connect4_genNonLosing(_c4, movArr, &movCnt);
    ttMov ? NegaScout_promote(movArr, movCnt, ~ttMov), NegaScout_History_sort(movArr, 1, movCnt - 1) : FTW_VOID_NOP;

    const int16_t OLD_A = _a;

    for (uint8_t i = 0; i < movCnt; i++)
    {
        const uint8_t MOV = movArr[i];

        Connect4_play(_c4, MOV);

        i ? (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_misere_search(_c4, _D - 1, -_a - 1, -_a)),
             curr = curr > _a && curr < _B ? NegaScout_adjustWinLoss(-NegaScout_Connect4_misere_search(_c4, _D - 1, -_B, -_a)) : curr)
          : (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_misere_search(_c4, _D - 1, -_B, -_a)));

        Connect4_unplay(_c4);

        if (curr > best)
        {
            best = curr;
            move = MOV;

            if (best > _a)
            {
                _a = best;

                if (_a >= _B)
                {
                    NS_hist[move] += _D * _D;

                    break;
                }
            }
        }
    }

    TransBucket_store(tb, LOCK, best, _D, OLD_A, _B, move);

    return best;
}

////////////////////////////////////////////////////////
/// @brief  NegaScout search on Connect 4 PopOut.
/// @param  _c4
/// @param  _D
/// @param  _a
/// @param  _B
/// @note   Requires graph semantics to resolve cycles.
////////////////////////////////////////////////////////
static inline int16_t NegaScout_Connect4_popout_search(Connect4 *const restrict _c4, const uint16_t _D, int16_t _a, const int16_t _B)
{
    NS_nodes++;

    if (Connect4_canWin(_c4))
    {
        return NS_WIN_VAL;
    }

    if (FTW_BRANCH_COLD(Connect4_popout_popLose(_c4)))
    {
        return -NS_WIN_VAL;
    }

    if (!_D)
    {
        return -NS_PROG;
    }

    const Board KEY = Connect4_key(_c4);
#ifdef FTW_TT_128_BITS
#ifdef FTW_XXHASH
    const XXH128_hash_t HASH = XXH3_128bits_withSeed(&_c4->plies, sizeof(_c4->plies), C4_xxhSeed);
    const TTLock MIX = HASH.low64 | (TTLock)(HASH.high64) << 64;
#else
    const Murmur128 HASH = Murmur3_x64_128bits(&_c4->plies, sizeof(_c4->plies), C4_SALT_A);
    const TTLock MIX = HASH.h2 | (TTLock)(HASH.h1) << 64;
#endif
#else
#ifdef FTW_XXHASH
    const uint64_t MIX = XXH3_64bits_withSeed(&_c4->plies, sizeof(_c4->plies), C4_xxhSeed);
#else
    const uint64_t MIX = SplitMix64_finalize(_c4->plies);
#endif
#endif
    const TTLock LOCK = KEY ^ MIX; int16_t ttVal; uint8_t ttMov;

    TransBucket *const restrict tb = &NS_table.bucket[TransTable_index(&NS_table, LOCK)];

    if (TransBucket_probe(tb, LOCK, _D, _a, _B, &ttVal, &ttMov) == TT_CUT)
    {
        return ttVal;
    }

    PathEntry *const restrict pe = PathTable_probe(&NS_path, KEY); pe->key = KEY;

    if (PathEntry_backedge(pe, KEY))
    {
        //PathGraph_addEdge(&NS_graph, &NS_path, pe, NS_stack.data[NS_stack.top], MIX);

        return NS_DRAW; // Not mathematically correct, but avoids infinite descent
    }

    PathEntry_push(pe);
    //PathStack_push(&NS_stack, KEY);

    int16_t curr, best = -NS_WIN_VAL;
    uint8_t movArr[MOVE_BOUNDS], movCnt, move = UINT8_MAX;

    Connect4_genNonLosing(_c4, movArr, &movCnt);
    ttMov ? NegaScout_promote(movArr, movCnt, ~ttMov), NegaScout_History_sort(movArr, 1, movCnt - 1) : FTW_VOID_NOP;

    const int16_t OLD_A = _a;

    for (uint8_t i = 0; i < movCnt; i++)
    {
        const uint8_t MOV = movArr[i];

        Connect4_play(_c4, MOV);

        i ? (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_popout_search(_c4, _D - 1, -_a - 1, -_a)),
             curr = curr > _a && curr < _B ? NegaScout_adjustWinLoss(-NegaScout_Connect4_popout_search(_c4, _D - 1, -_B, -_a)) : curr)
          : (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_popout_search(_c4, _D - 1, -_B, -_a)));

        Connect4_unplay(_c4);

        if (curr > best)
        {
            best = curr;
            move = MOV;

            if (best > _a)
            {
                _a = best;

                if (_a >= _B)
                {
                    NS_hist[move] += _D * _D;

                    break;
                }
            }
        }
    }

    TransBucket_store(tb, LOCK, best, _D, OLD_A, _B, move);
    PathEntry_pop(pe);
    //PathStack_pop(&NS_stack);

    return best;
}

//////////////////////////////////////////////////
/// @brief  NegaScout search on Connect 4 Pop 10.
/// @param  _c4
/// @param  _p10
/// @param  _D
/// @param  _a
/// @param  _B
/// @note   EXPERIMENTAL
//////////////////////////////////////////////////
static inline int16_t NegaScout_Connect4_pop10_search(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const uint16_t _D, int16_t _a, const int16_t _B)
{
    NS_nodes++;

    if (_p10->phase && Connect4_pop10_canWin(_c4->side, _p10->turn ? _p10->pops >> 4 : _p10->pops & 0xf))
    {
        return NS_WIN_VAL;
    }

    if (!_D)
    {
        return -NS_PROG;
    }

    const Board KEY = Connect4_key(_c4) | Connect4_pop10_key(_p10);
#ifdef FTW_TT_128_BITS
#ifdef FTW_XXHASH
    const XXH128_hash_t HASH = XXH3_128bits_withSeed(&_c4->plies, sizeof(_c4->plies), C4_xxhSeed);
    const TTLock MIX = HASH.low64 | (TTLock)(HASH.high64) << 64;
#else
    const Murmur128 HASH = Murmur3_x64_128bits(&_c4->plies, sizeof(_c4->plies), C4_SALT_A);
    const TTLock MIX = HASH.h2 | (TTLock)(HASH.h1) << 64;
#endif
#else
#ifdef FTW_XXHASH
    const uint64_t MIX = XXH3_64bits_withSeed(&_c4->plies, sizeof(_c4->plies), C4_xxhSeed);
#else
    const uint64_t MIX = SplitMix64_finalize(_c4->plies);
#endif
#endif
    const TTLock LOCK = KEY ^ MIX; int16_t ttVal; uint8_t ttMov;

    TransBucket *const restrict tb = &NS_table.bucket[TransTable_index(&NS_table, LOCK)];

    if (TransBucket_probe(tb, LOCK, _D, _a, _B, &ttVal, &ttMov) == TT_CUT)
    {
        return ttVal;
    }

    PathEntry *const restrict pe = PathTable_probe(&NS_path, KEY); pe->key = KEY;

    if (PathEntry_backedge(pe, KEY))
    {
        //PathGraph_addEdge(&NS_graph, &NS_path, pe, NS_stack.data[NS_stack.top], MIX);

        return NS_DRAW;
    }

    PathEntry_push(pe);
    //PathStack_push(&NS_stack, KEY);

    int16_t curr, best = -NS_WIN_VAL;
    uint8_t movArr[MOVE_BOUNDS], movCnt, move = UINT8_MAX;

    Connect4_pop10_generate(_c4, _p10, movArr, &movCnt);
    ttMov ? NegaScout_promote(movArr, movCnt, ~ttMov), NegaScout_History_sort(movArr, 1, movCnt - 1) : FTW_VOID_NOP;

    const int16_t OLD_A = _a;
    const bool OLD_TURN = _p10->turn;

    for (uint8_t i = 0; i < movCnt; i++)
    {
        const uint8_t MOV = movArr[i];

        Connect4_pop10_play(_c4, _p10, MOV);

        if (_p10->turn == OLD_TURN)
        {
            i ? (curr = NegaScout_adjustWinLoss(NegaScout_Connect4_pop10_search(_c4, _p10, _D - 1, _a, _a + 1)),
                curr = curr > _a && curr < _B ? NegaScout_adjustWinLoss(NegaScout_Connect4_pop10_search(_c4, _p10, _D - 1, _a, _B)) : curr)
              : (curr = NegaScout_adjustWinLoss(NegaScout_Connect4_pop10_search(_c4, _p10, _D - 1, _a, _B)));
        }
        else
        {
            i ? (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_pop10_search(_c4, _p10, _D - 1, -_a - 1, -_a)),
                curr = curr > _a && curr < _B ? NegaScout_adjustWinLoss(-NegaScout_Connect4_pop10_search(_c4, _p10, _D - 1, -_B, -_a)) : curr)
              : (curr = NegaScout_adjustWinLoss(-NegaScout_Connect4_pop10_search(_c4, _p10, _D - 1, -_B, -_a)));
        }

        Connect4_pop10_unplay(_c4, _p10);

        if (curr > best)
        {
            best = curr;
            move = MOV;

            if (best > _a)
            {
                _a = best;

                if (_a >= _B)
                {
                    NS_hist[move] += _D * _D;

                    break;
                }
            }
        }
    }

    TransBucket_store(tb, LOCK, best, _D, OLD_A, _B, move);
    PathEntry_pop(pe);
    //PathStack_pop(&NS_stack);

    return best;
}

///////////////////////////////////////////////////
/// @brief  NegaScout algorithm applied to Make 7.
/// @param  _M7
/// @param  _D
/// @param  _a
/// @param  _B
///////////////////////////////////////////////////
static inline int16_t NegaScout_Make7_search(const Make7 *const restrict _M7, const uint16_t _D, int16_t _a, const int16_t _B)
{
    NS_nodes++;

    if (Make7_canWin(_M7))
    {
        return NS_WIN_VAL;
    }

    if (FTW_BRANCH_COLD(Make7_noMoreTiles(_M7)))
    {
        return NS_DRAW;
    }

    if (!_D)
    {
        return -NS_PROG;
    }

    int16_t ttVal; uint8_t ttMov;

    const TTLock LOCK = Make7_lock(_M7);

    TransBucket *const restrict tb = &NS_table.bucket[TransTable_index(&NS_table, LOCK)];

    if (TransBucket_probe(tb, LOCK, _D, _a, _B, &ttVal, &ttMov) == TT_CUT)
    {
        return ttVal;
    }

    int16_t curr, best = -NS_WIN_VAL;
    uint8_t movArr[MAKE7_SIZE_X3], movCnt, move = UINT8_MAX;

    Make7_generate(_M7, movArr, &movCnt);
    ttMov ? NegaScout_promote(movArr, movCnt, ~ttMov), NegaScout_History_sort(movArr, 1, movCnt - 1) : FTW_VOID_NOP;

    const int16_t OLD_A = _a;

    for (uint8_t i = 0; i < movCnt; i++)
    {
        const uint8_t MOV = movArr[i];

        Make7 scoutM7 = *_M7;

        Make7_drop(&scoutM7, MOV >> 3, MOV & 7);

        i ? (curr = NegaScout_adjustWinLoss(-NegaScout_Make7_search(&scoutM7, _D - 1, -_a - 1, -_a)),
             curr = curr > _a && curr < _B ? NegaScout_adjustWinLoss(-NegaScout_Make7_search(&scoutM7, _D - 1, -_B, -_a)) : curr)
          : (curr = NegaScout_adjustWinLoss(-NegaScout_Make7_search(&scoutM7, _D - 1, -_B, -_a)));

        if (curr > best)
        {
            best = curr;
            move = MOV;

            if (best > _a)
            {
                _a = best;

                if (_a >= _B)
                {
                    NS_hist[move] += _D * _D;

                    break;
                }
            }
        }
    }

    TransBucket_store(tb, LOCK, best, _D, OLD_A, _B, move);

    return best;
}

///////////////////////////////////////////////////////////////////
/// @brief          Iterative deepening NegaScout on Connect 4.
/// @param _c4      Unaliased pointer to the Connect 4 structure.
/// @param _PRINT   Whether to print the current search progress.
/// @return         Distance to win/loss in plies; zero for draws.
///////////////////////////////////////////////////////////////////
static inline Result NegaScout_Connect4_original_iterative(Connect4 *const restrict _c4, const bool _PRINT)
{
    NS_nodes = 0;

    const uint16_t DEPTH = PLY_LENGTH - _c4->plies - 1;

    uint16_t i; int16_t val = NS_DRAW;

    TransTable_reset(&NS_table);
    NegaScout_History_randomize();
    FourTheWin_monoTime(&NS_str);

    for (i = 0; i <= DEPTH; i++)
    {
        _PRINT ? printf(FTW_STR_SOLVING_FINITE, i, DEPTH), fflush(stdout) : FTW_VOID_NOP;

        if (abs((val = NegaScout_Connect4_search(_c4, i, -NS_PROG, NS_PROG))) >= NS_WIN_THR)
        {
            break;
        }

        NegaScout_History_decay();
    }

    FourTheWin_monoTime(&NS_end);
    NS_time = (double)(NS_end.tv_sec - NS_str.tv_sec) + (double)(NS_end.tv_nsec - NS_str.tv_nsec) / 1e9;

    return Result_fromNegaScout(val > NS_DRAW ? NS_WIN_VAL + 1 - val : val < NS_DRAW ? -NS_WIN_VAL - 1 - val : val, NS_nodes, NS_time);
}

////////////////////////////////////////////////////////////
/// @brief  Connect 4 PopOut iterative deepening NegaScout.
/// @param  _c4
/// @param  _PRINT
////////////////////////////////////////////////////////////
static inline Result NegaScout_Connect4_popout_iterative(Connect4 *const restrict _c4, const bool _PRINT)
{
    NS_nodes = 0;

    const uint16_t POP_DEPTH = BOARD_AREA < 128 ? UINT8_MAX : NS_MAX_DEPTH;
    const uint16_t DEPTH = _c4->plies > POP_DEPTH ? _c4->plies : POP_DEPTH;

    // uint64_t nodePrev = 0, nodeDelta = 0; int16_t val = NS_DRAW;
    int16_t val = NS_DRAW, vals[2] = { -NS_PROG, NS_PROG };

    TransTable_reset(&NS_table);
    NegaScout_History_randomize();
    FourTheWin_monoTime(&NS_str);

    for (uint16_t i = 0; i <= DEPTH; i++)
    {
        _PRINT ? printf(FTW_STR_SOLVING_INFINITE, i), fflush(stdout) : FTW_VOID_NOP;

        if (abs((val = NegaScout_Connect4_search(_c4, i, -NS_PROG, NS_PROG))) >= NS_WIN_THR)
        {
            goto NegaScout_Connect4_popout_iterative_solved;
        }

        /*if (NS_graph.size)
        {
            PathGraph_Tarjan(&NS_graph, &NS_table);
            PathGraph_destroyEdges(&NS_graph);
        }*/

        //PathGraph_age(&NS_graph);
        //const uint64_t DELTA = NS_nodes - nodePrev;

        vals[i & 1] = val;

        if (val == NS_DRAW && vals[0] == vals[1]) // || DELTA == nodeDelta))
        {
            goto NegaScout_Connect4_popout_iterative_solved;
        }

        //nodePrev = NS_nodes;
        //nodeDelta = DELTA;

        NegaScout_History_decay();
    }

NegaScout_Connect4_popout_iterative_solved:
    FourTheWin_monoTime(&NS_end);

    NS_time = (double)(NS_end.tv_sec - NS_str.tv_sec) + (double)(NS_end.tv_nsec - NS_str.tv_nsec) / 1e9;

    return Result_fromNegaScout(val > NS_DRAW ? NS_WIN_VAL + 1 - val : val < NS_DRAW ? -NS_WIN_VAL - 1 - val : val, NS_nodes, NS_time);
}

///////////////////////////////////////////////////////////////////
/// @brief  An iterative deepening NegaScout for Connect 4 Pop 10.
/// @param  _c4
/// @param  _p10
/// @param  _PRINT
///////////////////////////////////////////////////////////////////
static inline Result NegaScout_Connect4_pop10_iterative(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const bool _PRINT)
{
    NS_nodes = 0;

    const uint16_t DEPTH = _c4->plies > NS_MAX_DEPTH ? _c4->plies : NS_MAX_DEPTH;

    //uint64_t nodePrev = 0, nodeDelta = 0; int16_t val = NS_DRAW;
    int16_t val = NS_DRAW, vals[2] = { -NS_PROG, NS_PROG };

    TransTable_reset(&NS_table);
    NegaScout_History_randomize();
    FourTheWin_monoTime(&NS_str);

    if (Connect4_pop10_draw(_c4, _p10))
    {
        goto NegaScout_Connect4_pop10_iterative_solved;
    }

    for (uint16_t i = 0; i <= DEPTH; i++)
    {
        _PRINT ? printf(FTW_STR_SOLVING_INFINITE, i), fflush(stdout) : FTW_VOID_NOP;

        if (abs((val = NegaScout_Connect4_pop10_search(_c4, _p10, i, -NS_PROG, NS_PROG))) >= NS_WIN_THR)
        {
            goto NegaScout_Connect4_pop10_iterative_solved;
        }

        /*if (NS_graph.size)
        {
            PathGraph_Tarjan(&NS_graph, &NS_table);
            PathGraph_destroyEdges(&NS_graph);
        }

        PathGraph_age(&NS_graph);*/

        /*const uint64_t DELTA = NS_nodes - nodePrev;

        if (!val && DELTA == nodeDelta)
        {
            goto NegaScout_Connect4_pop10_iterative_solved;
        }*/

        vals[i & 1] = val;

        if (val == NS_DRAW && vals[0] == vals[1])
        {
            goto NegaScout_Connect4_pop10_iterative_solved;
        }

        /*nodePrev = NS_nodes;
        nodeDelta = DELTA;*/

        NegaScout_History_decay();
    }

NegaScout_Connect4_pop10_iterative_solved:
    FourTheWin_monoTime(&NS_end);

    NS_time = (double)(NS_end.tv_sec - NS_str.tv_sec) + (double)(NS_end.tv_nsec - NS_str.tv_nsec) / 1e9;

    return Result_fromNegaScout(val > NS_DRAW ? NS_WIN_VAL + 1 - val : val < NS_DRAW ? -NS_WIN_VAL - 1 - val : val, NS_nodes, NS_time);
}

//////////////////////////////////////////////////
/// @brief  Make 7 iterative deepening NegaScout.
/// @param  _M7
/// @param  _PRINT
//////////////////////////////////////////////////
static inline Result NegaScout_Make7_iterative(const Make7 *const restrict _M7, const bool _PRINT)
{
    NS_nodes = 0;

    const uint16_t DEPTH = MAKE7_AREA - Make7_moves(_M7) - 1;

    uint16_t i; int16_t val = NS_DRAW;

    TransTable_reset(&NS_table);
    NegaScout_History_randomize();
    FourTheWin_monoTime(&NS_str);

    for (i = 0; i <= DEPTH; i++)
    {
        _PRINT ? printf(FTW_STR_SOLVING_FINITE, i, DEPTH), fflush(stdout) : FTW_VOID_NOP;

        if (abs((val = NegaScout_Make7_search(_M7, i, -NS_PROG, NS_PROG))) >= NS_WIN_THR || val == NS_DRAW)
        {
            break;
        }

        NegaScout_History_decay();
    }

    FourTheWin_monoTime(&NS_end);
    NS_time = (double)(NS_end.tv_sec - NS_str.tv_sec) + (double)(NS_end.tv_nsec - NS_str.tv_nsec) / 1e9;

    return Result_fromNegaScout(val > NS_DRAW ? NS_WIN_VAL + 1 - val : val < NS_DRAW ? -NS_WIN_VAL - 1 - val : val, NS_nodes, NS_time);
}

/////////////////////////////////////////////////////////////////////
/// @brief          Analyzes every move and prints their results.
/// @param _c4      Unaliased pointer to the Connect 4 game state.
/// @param _PRINT   Whether to dampen (F) or display (T) the output.
/// @return         `true` if a move was found; otherwise `false`.
/////////////////////////////////////////////////////////////////////
static inline char NegaScout_Connect4_original_results(Connect4 *const restrict _c4, const bool _PRINT)
{
    Result resArr[MOVE_SPACE]; uint8_t i;

    for (i = 0; i < MOVE_SPACE; i++)
    {
        resArr[i] = RESULT_NULL;
    }

    for (i = 0; i < COLS; i++)
    {
        if (Connect4_playable(_c4, i))
        {
            Connect4_play(_c4, i);

            Result *const restrict res = &resArr[i];

            if (Connect4_fourInARow(_c4->side ^ _c4->mask))
            {
                *res = C4_variant == CONNECT4_MISERE ? RESULT_LOSS : RESULT_WIN;
            }
            else if (Connect4_full(_c4))
            {
                *res = RESULT_DRAW;
            }
            else
            {
#ifdef FTW_SQLITE
                const Board RES_KEY = Connect4_key(_c4);

                if (!SQLite_Connect4_query(database, RES_KEY, res))
#else
                const Board RES_KEY = Connect4_canonicalize(Connect4_key(_c4));

                *res = ResultRing_query(RES_KEY, UINT64_MAX, UINT64_MAX);

                if (res->wdl == NULL_CHAR)
#endif
                {
                    *res = NegaScout_Connect4_original_iterative(_c4, false);
#ifdef FTW_SQLITE
                    SQLite_Connect4_insert(database, RES_KEY, res);
#else
                    ResultRing_insert(RES_KEY, UINT64_MAX, UINT64_MAX, *res);
#endif
                }

                Result_increment(res);
            }

            if (_PRINT)
            {
                switch (res->wdl)
                {
                case WIN_CHAR:
                    printf(FTW_CHR_GREEN_FULL_BLOCK);
                    break;
                case DRAW_CHAR:
                    printf(FTW_CHR_YELLOW_FULL_BLOCK);
                    break;
                default:
                    printf(FTW_CHR_RED_FULL_BLOCK);
                    break;
                }
            }
            else
            {
                printf(FTW_CHR_FULL_BLOCK);
            }

            fflush(stdout);
            Connect4_unplay(_c4);
        }
    }

    const Result BEST_RES = Result_best(resArr, MOVE_SPACE);

#ifdef FTW_SQLITE
    SQLite_Connect4_insert(database, Connect4_key(_c4), &BEST_RES);
#else
    const Board ROOT_KEY = Connect4_canonicalize(Connect4_key(_c4));

    if (ResultRing_query(ROOT_KEY, UINT64_MAX, UINT64_MAX).wdl == NULL_CHAR)
    {
        ResultRing_insert(ROOT_KEY, UINT64_MAX, UINT64_MAX, BEST_RES);
    }
#endif

    putchar('\r');

    if (_PRINT)
    {
        for (i = 0; i < COLS; i++)
        {
            Result_print(resArr[i], &BEST_RES, true, false);
            putchar(' ');
        }

        putchar('\n');
    }

    char chosenChar = '\0', bestChars[MOVE_SPACE]; uint8_t bestCnt = 0;

    for (i = 0; i < MOVE_SPACE; i++)
    {
        if (resArr[i].wdl == BEST_RES.wdl && resArr[i].dtw == BEST_RES.dtw)
        {
            bestChars[bestCnt++] = i + '1';
        }
    }

    if (bestCnt)
    {
        chosenChar = bestChars[Xoshiro128pp_nextN(&g_rng, bestCnt)];
    }

    return chosenChar;
}

//////////////////////////////////////////////////////////////////////
/// @brief  PopOut flavor of `NegaScout_Connect4_original_results()`.
/// @param  _c4
/// @param  _PRINT
//////////////////////////////////////////////////////////////////////
static inline char NegaScout_Connect4_popout_results(Connect4 *const restrict _c4, const bool _PRINT)
{
    Result resDrops[COLS], resPops[COLS], *restrict res;
    Board resKey; uint8_t i;

    for (i = 0; i < COLS; i++)
    {
        resDrops[i] = resPops[i] = RESULT_NULL;
    }

    for (i = 0; i < COLS; i++)
    {
        if (Connect4_droppable(_c4, i))
        {
            Connect4_drop(_c4, i);

            res = &resDrops[i];

            if (Connect4_fourInARow(_c4->side ^ _c4->mask))
            {
                *res = RESULT_WIN;
            }
            else
            {
#ifdef FTW_SQLITE
                resKey = Connect4_key(_c4);

                if (!SQLite_Connect4_query(database, resKey, res))
#else
                resKey = Connect4_canonicalize(Connect4_key(_c4));
                *res = ResultRing_query(resKey, UINT64_MAX, UINT64_MAX);

                if (res->wdl == NULL_CHAR)
#endif
                {
                    *res = NegaScout_Connect4_popout_iterative(_c4, false);
#ifdef FTW_SQLITE
                    SQLite_Connect4_insert(database, resKey, res);
#else
                    ResultRing_insert(resKey, UINT64_MAX, UINT64_MAX, *res);
#endif
                }

                Result_increment(res);
            }

            if (_PRINT)
            {
                switch (res->wdl)
                {
                case WIN_CHAR:
                    printf(FTW_CHR_GREEN_UPPER_BLOCK);
                    break;
                case DRAW_CHAR:
                    printf(FTW_CHR_YELLOW_UPPER_BLOCK);
                    break;
                default:
                    printf(FTW_CHR_RED_UPPER_BLOCK);
                    break;
                }
            }
            else
            {
                printf(FTW_CHR_UPPER_BLOCK);
            }

            fflush(stdout);
            Connect4_undrop(_c4);
        }

        if (Connect4_poppable(_c4, i))
        {
            Connect4_pop(_c4, i);

            res = &resPops[i];

            if (Connect4_fourInARow(_c4->side ^ _c4->mask))
            {
                *res = RESULT_WIN;
            }
            else if (Connect4_fourInARow(_c4->side))
            {
                *res = RESULT_LOSS;
            }
            else
            {
#ifdef FTW_SQLITE
                resKey = Connect4_key(_c4);

                if (!SQLite_Connect4_query(database, resKey, res))
#else
                resKey = Connect4_canonicalize(Connect4_key(_c4));
                *res = ResultRing_query(resKey, UINT64_MAX, UINT64_MAX);

                if (res->wdl == NULL_CHAR)
#endif
                {
                    *res = NegaScout_Connect4_popout_iterative(_c4, false);
#ifdef FTW_SQLITE
                    SQLite_Connect4_insert(database, resKey, res);
#else
                    ResultRing_insert(resKey, UINT64_MAX, UINT64_MAX, *res);
#endif
                }

                Result_increment(res);
            }

            if (_PRINT)
            {
                switch (res->wdl)
                {
                case WIN_CHAR:
                    printf(FTW_CHR_GREEN_LOWER_BLOCK);
                    break;
                case DRAW_CHAR:
                    printf(FTW_CHR_YELLOW_LOWER_BLOCK);
                    break;
                default:
                    printf(FTW_CHR_RED_LOWER_BLOCK);
                    break;
                }
            }
            else
            {
                printf(FTW_CHR_LOWER_BLOCK);
            }

            fflush(stdout);
            Connect4_unpop(_c4);
        }
    }

    Result resArr[2];

    resArr[0] = Result_best(resDrops, COLS);
    resArr[1] = Result_best(resPops, COLS);

    const Result GLB_RES = Result_best(resArr, 2);

#ifdef FTW_SQLITE
    SQLite_Connect4_insert(database, Connect4_key(_c4), &GLB_RES);
#else
    const Board ROOT_KEY = Connect4_canonicalize(Connect4_key(_c4));

    if (ResultRing_query(ROOT_KEY, UINT64_MAX, UINT64_MAX).wdl == NULL_CHAR)
    {
        ResultRing_insert(ROOT_KEY, UINT64_MAX, UINT64_MAX, GLB_RES);
    }
#endif

    putchar('\r');

    if (_PRINT)
    {
        for (i = 0; i < COLS; i++)
        {
            Result_print(resDrops[i], &GLB_RES, true, false);
            putchar(' ');
        }

        putchar('\n');

        for (i = 0; i < COLS; i++)
        {
            Result_print(resPops[i], &GLB_RES, true, false);
            putchar(' ');
        }

        putchar('\n');
    }

    char chosenChar = '\0', globalChar[MOVE_BOUNDS]; uint8_t globalCnt = 0;

    for (i = 0; i < COLS; i++)
    {
        if (resDrops[i].wdl == GLB_RES.wdl && resDrops[i].dtw == GLB_RES.dtw)
        {
            globalChar[globalCnt++] = i + (COLS < 10 ? '1' : 'A');
        }

        if (resPops[i].wdl == GLB_RES.wdl && resPops[i].dtw == GLB_RES.dtw)
        {
            globalChar[globalCnt++] = i + (COLS < 10 ? 'A' : 'a');
        }
    }

    if (globalCnt)
    {
        chosenChar = globalChar[Xoshiro128pp_nextN(&g_rng, globalCnt)];
    }

    return chosenChar;
}

//////////////////////////////////////////////////////////////////
/// @brief  Pop 10 translation of `NegaScout_Connect4_results()`.
/// @param  _c4
/// @param  _p10
/// @param  _PRINT
//////////////////////////////////////////////////////////////////
static inline char NegaScout_Connect4_pop10_results(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const bool _PRINT)
{
    Result resArr[COLS], *restrict res;
    Board resKeyA, resKeyB; uint8_t i;

    for (i = 0; i < COLS; i++)
    {
        resArr[i] = RESULT_NULL;
    }

    const bool PASS_FORCED = Connect4_pop10_passForced(_c4, _p10);

    if (PASS_FORCED)
    {
        Connect4_pop10_passMove(_c4, _p10);

        res = &resArr[0];

#ifdef FTW_SQLITE
        resKeyA = Connect4_key(_c4);
#else
        resKeyA = Connect4_canonicalize(Connect4_key(_c4));
#endif
        resKeyB = Connect4_pop10_stateKey(_p10);

#ifdef FTW_SQLITE
        if (!SQLite_Connect4_pop10_query(database, resKeyA, resKeyB, res))
#else
        *res = ResultRing_query(resKeyA, resKeyB, UINT64_MAX);

        if (res->wdl == NULL_CHAR)
#endif
        {
            *res = NegaScout_Connect4_pop10_iterative(_c4, _p10, false);

#ifdef FTW_SQLITE
            SQLite_Connect4_pop10_insert(database, resKeyA, resKeyB, res);
#else
            ResultRing_insert(resKeyA, resKeyB, UINT64_MAX, *res);
#endif
        }

        Result_increment(res);

        if (_PRINT)
        {
            switch (res->wdl)
            {
            case WIN_CHAR:
                printf(FTW_CHR_GREEN_FULL_BLOCK);
                break;
            case DRAW_CHAR:
                printf(FTW_CHR_YELLOW_FULL_BLOCK);
                break;
            default:
                printf(FTW_CHR_RED_FULL_BLOCK);
                break;
            }
        }
        else
        {
            printf(FTW_CHR_FULL_BLOCK);
        }

        fflush(stdout);
        Connect4_pop10_unpassMove(_c4, _p10);
    }
    else
    {
        const uint8_t NON_FULLS = Connect4_nonFullCols(_c4);
        const uint8_t HIST_COL = _c4->hist[_c4->plies - 1] - COLS;

        for (i = 0; i < COLS; i++)
        {
            if (_p10->phase)
            {
                if (Connect4_poppable(_c4, i))
                {
                    Connect4_pop10_pop(_c4, _p10, i);

                    res = &resArr[i];

                    if (Connect4_pop10_over(_p10))
                    {
                        *res = RESULT_WIN;
                    }
                    else
                    {
#ifdef FTW_SQLITE
                        resKeyA = Connect4_key(_c4);
#else
                        resKeyA = Connect4_canonicalize(Connect4_key(_c4));
#endif
                        resKeyB = Connect4_pop10_stateKey(_p10);

#ifdef FTW_SQLITE
                        if (!SQLite_Connect4_pop10_query(database, resKeyA, resKeyB, res))
#else
                        *res = ResultRing_query(resKeyA, resKeyB, UINT64_MAX);

                        if (res->wdl == NULL_CHAR)
#endif
                        {
                            *res = NegaScout_Connect4_pop10_iterative(_c4, _p10, false);
#ifdef FTW_SQLITE
                            SQLite_Connect4_pop10_insert(database, resKeyA, resKeyB, res);
#else
                            ResultRing_insert(resKeyA, resKeyB, UINT64_MAX, *res);
#endif
                        }

                        res->dtw++;
                    }

                    if (_PRINT)
                    {
                        switch (res->wdl)
                        {
                        case WIN_CHAR:
                            printf(FTW_CHR_GREEN_FULL_BLOCK);
                            break;
                        case DRAW_CHAR:
                            printf(FTW_CHR_YELLOW_FULL_BLOCK);
                            break;
                        default:
                            printf(FTW_CHR_RED_FULL_BLOCK);
                            break;
                        }
                    }
                    else
                    {
                        printf(FTW_CHR_FULL_BLOCK);
                    }

                    fflush(stdout);
                    Connect4_pop10_unpop(_c4, _p10);
                }
            }
            else
            {
                if ((NON_FULLS == 1 || (NON_FULLS >= 2 && i != HIST_COL)) && Connect4_droppable(_c4, i))
                {
                    Connect4_pop10_drop(_c4, _p10, i);

                    res = &resArr[i];
#ifdef FTW_SQLITE
                    resKeyA = Connect4_key(_c4);
#else
                    resKeyA = Connect4_canonicalize(Connect4_key(_c4));
#endif
                    resKeyB = Connect4_pop10_stateKey(_p10);

#ifdef FTW_SQLITE
                    if (!SQLite_Connect4_pop10_query(database, resKeyA, resKeyB, res))
#else
                    *res = ResultRing_query(resKeyA, resKeyB, UINT64_MAX);

                    if (res->wdl == NULL_CHAR)
#endif
                    {
                        *res = NegaScout_Connect4_pop10_iterative(_c4, _p10, false);
#ifdef FTW_SQLITE
                        SQLite_Connect4_pop10_insert(database, resKeyA, resKeyB, res);
#else
                        ResultRing_insert(resKeyA, resKeyB, UINT64_MAX, *res);
#endif
                    }

                    Result_increment(res);

                    if (_PRINT)
                    {
                        switch (res->wdl)
                        {
                        case WIN_CHAR:
                            printf(FTW_CHR_GREEN_FULL_BLOCK);
                            break;
                        case DRAW_CHAR:
                            printf(FTW_CHR_YELLOW_FULL_BLOCK);
                            break;
                        default:
                            printf(FTW_CHR_RED_FULL_BLOCK);
                            break;
                        }
                    }
                    else
                    {
                        printf(FTW_CHR_FULL_BLOCK);
                    }

                    fflush(stdout);
                    Connect4_pop10_undrop(_c4, _p10);
                }
            }
        }
    }

    const Result BEST_RES = Result_best(resArr, COLS);
    const Board ROOT_STATE = Connect4_pop10_stateKey(_p10);

#ifdef FTW_SQLITE
    SQLite_Connect4_pop10_insert(database, Connect4_key(_c4), ROOT_STATE, &BEST_RES);
#else
    const Board ROOT_KEY = Connect4_canonicalize(Connect4_key(_c4));

    if (ResultRing_query(ROOT_KEY, ROOT_STATE, UINT64_MAX).wdl == NULL_CHAR)
    {
        ResultRing_insert(ROOT_KEY, ROOT_STATE, UINT64_MAX, BEST_RES);
    }
#endif

    putchar('\r');

    if (_PRINT)
    {
        for (i = 0; i < COLS; i++)
        {
            Result_print(resArr[i], &BEST_RES, true, false);
            putchar(' ');
        }

        putchar('\n');
    }

    char chosenChar = '\0', bestChars[MOVE_SPACE]; uint8_t bestCnt = 0;

    for (i = 0; i < COLS; i++)
    {
        if (resArr[i].wdl == BEST_RES.wdl && resArr[i].dtw == BEST_RES.dtw)
        {
            bestChars[bestCnt++] = PASS_FORCED ? POP10_PASS_CHR : i + (_p10->phase ? (COLS < 10 ? 'A' : 'a') : (COLS < 10 ? '1' : 'A'));
        }
    }

    if (bestCnt)
    {
        chosenChar = bestChars[Xoshiro128pp_nextN(&g_rng, bestCnt)];
    }

    return chosenChar;
}

////////////////////////////////////////////////////
/// @brief  Move-wide results for NegaScout Make 7.
/// @param  _M7
/// @param  _PRINT
/// @param  _mov
////////////////////////////////////////////////////
static inline Result NegaScout_Make7_results(const Make7 *const restrict _M7, const bool _PRINT, char _mov[const restrict static 2])
{
    Result m7Res[MAKE7_SIZE_X3], *restrict res; uint8_t i, j;

    for (i = 0; i < MAKE7_SIZE_X3; i++)
    {
        m7Res[i] = RESULT_NULL;
    }

    uint64_t m7Key;

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < MAKE7_SIZE; j++)
        {
            res = &m7Res[j] + (i * MAKE7_SIZE);

            Make7 copyM7 = *_M7;

            if (Make7_droppable(&copyM7, i, j))
            {
                Make7_drop(&copyM7, i, j);

                if (Make7_targetSum(&copyM7))
                {
                    *res = RESULT_WIN;
                }
                else
                {
                    m7Key = Make7_partKey(&copyM7);
#ifdef FTW_SQLITE
                    if (!SQLite_Make7_query(database, m7Key, copyM7.tile1, copyM7.tile2, M7_targetMode, res))
#else
                    uint64_t t1Key = copyM7.tile1, t2Key = copyM7.tile2;

                    Make7_canonicalize(m7Key, t1Key, t2Key, &m7Key, &t1Key, &t2Key);
                    *res = ResultRing_query(m7Key, t1Key, t2Key);

                    if (res->wdl == NULL_CHAR)
#endif
                    {
                        *res = NegaScout_Make7_iterative(&copyM7, false);
#ifdef FTW_SQLITE
                        SQLite_Make7_insert(database, m7Key, copyM7.tile1, copyM7.tile2, M7_targetMode, res);
#else
                        ResultRing_insert(m7Key, t1Key, t2Key, *res);
#endif
                    }

                    Result_increment(res);

                    if (_PRINT)
                    {
                        switch (res->wdl)
                        {
                        case WIN_CHAR:
                            printf(FTW_CHR_GREEN_FULL_BLOCK);
                            break;
                        case DRAW_CHAR:
                            printf(FTW_CHR_YELLOW_FULL_BLOCK);
                            break;
                        default:
                            printf(FTW_CHR_RED_FULL_BLOCK);
                            break;
                        }
                    }
                    else
                    {
                        printf(FTW_CHR_FULL_BLOCK);
                    }
                }

                fflush(stdout);
            }
        }
    }

    const Result M7_BEST = Result_best(m7Res, MAKE7_SIZE_X3);

#ifdef FTW_SQLITE
    SQLite_Make7_insert(database, Make7_partKey(_M7), _M7->tile1, _M7->tile2, M7_targetMode, &M7_BEST);
#else
    uint64_t m7pKey = Make7_partKey(_M7), m7t1Key = _M7->tile1, m7t2Key = _M7->tile2;

    Make7_canonicalize(m7pKey, m7t1Key, m7t2Key, &m7pKey, &m7t1Key, &m7t2Key);

    if (ResultRing_query(m7pKey, m7t1Key, m7t2Key).wdl == NULL_CHAR)
    {
        ResultRing_insert(m7pKey, m7t1Key, m7t2Key, M7_BEST);
    }
#endif

    putchar('\r');

    if (_PRINT)
    {
        for (i = 0; i < MAKE7_SIZE_X3; i++)
        {
            Result_print(m7Res[i], &M7_BEST, true, false);
            putchar(' ');

            if (i && i % MAKE7_SIZE == MAKE7_SIZE_M1)
            {
                putchar('\n');
            }
        }
    }

    char bestMovs[MAKE7_SIZE_X3]; uint8_t bestCnt = 0;

    for (i = 0; i < MAKE7_SIZE_X3; i++)
    {
        if (m7Res[i].wdl == M7_BEST.wdl && m7Res[i].dtw == M7_BEST.dtw)
        {
            bestMovs[bestCnt++] = (i / MAKE7_SIZE) << 3 | (i % MAKE7_SIZE);
        }
    }

    if (bestCnt)
    {
        const uint8_t BEST_MOV = bestMovs[Xoshiro128pp_nextN(&g_rng, bestCnt)];

        _mov[0] = (BEST_MOV >> 3) + '1';
        _mov[1] = (BEST_MOV & 7) + 'A';
    }

    return M7_BEST;
}

//////////////////////////////////////////////////////////////////////
/// @brief  Function pointer initialization for the NegaScout engine.
//////////////////////////////////////////////////////////////////////
static inline void NegaScout_funcPtrs_init(void)
{
    NegaScout_Connect4_search = NegaScout_Connect4_original_search;
    NegaScout_Connect4_iterative = NegaScout_Connect4_original_iterative;
    NegaScout_Connect4_results = NegaScout_Connect4_original_results;

    switch (C4_variant)
    {
    case CONNECT4_MISERE:
        NegaScout_Connect4_search = NegaScout_Connect4_misere_search;
        break;
    case CONNECT4_POPOUT:
        NegaScout_Connect4_search = NegaScout_Connect4_popout_search;
        NegaScout_Connect4_iterative = NegaScout_Connect4_popout_iterative;
        NegaScout_Connect4_results = NegaScout_Connect4_popout_results;
    default:
        break;
    }
}

#ifdef FTW_PGO
/////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Runs a NegaScout instance on all Connect 4 variants for profile-guided optimization.
/////////////////////////////////////////////////////////////////////////////////////////////////
static inline void NegaScout_Connect4_PGO(void)
{
    struct timespec profStart, profEnd;
    Connect4 profC4; Connect4_Pop10 profP10; Make7 profM7;

    constexpr uint8_t PGO_DEPS[5] = { 12, 12, 12, 10, 11};

    TransTable_init(&NS_table, TT_BASE_SIZE);
    PathTable_init(&NS_path, UINT16_MAX);
    //PathStack_init(&NS_stack);
    //PathGraph_init(&NS_graph);
    Xoshiro128_init(&g_rng);

    //NS_graph.searchId++;

    FourTheWin_monoTime(&profStart);

    for (C4_variant = CONNECT4_ORIGINAL; C4_variant <= CONNECT4_POP10; C4_variant++)
    {
        const bool PGO_POP10 = C4_variant == CONNECT4_POP10;

        printf(FTW_STR_PGO_START, FTW_STR_CONNECT4, PGO_POP10 ? FTW_STR_POP10 : FTW_STR_VARIANTS[C4_variant]);

        Connect4_prepare(7, 6);
        Connect4_init(&profC4);
        PGO_POP10 ? Connect4_pop10_reset(&profC4, &profP10) : FTW_VOID_NOP;
        Connect4_funcPtrs_init();
        Connect4_globals_init();

        NegaScout_funcPtrs_init();
        NegaScout_History_init();

        uint8_t colArr[MOVE_BOUNDS], colNum;

        for (uint8_t i = 0; i < 100; i++)
        {
            while (PGO_POP10 ? !Connect4_pop10_over(&profP10) : !Connect4_over(&profC4))
            {
                switch (C4_variant)
                {
                default:
                    NegaScout_Connect4_search(&profC4, PGO_DEPS[C4_variant], -NS_WIN_VAL, NS_WIN_VAL);
                    Connect4_genNonLosing(&profC4, colArr, &colNum);
                    break;
                case CONNECT4_POP10:
                    NegaScout_Connect4_pop10_search(&profC4, &profP10, PGO_DEPS[C4_variant], -NS_WIN_VAL, NS_WIN_VAL);
                    Connect4_pop10_generate(&profC4, &profP10, colArr, &colNum);
                    break;
                }

                if (colNum)
                {
                    switch (C4_variant)
                    {
                    default:
                        Connect4_play(&profC4, colArr[Xoshiro128pp_nextN(&g_rng, colNum)]);
                        break;
                    case CONNECT4_POP10:
                        Connect4_pop10_play(&profC4, &profP10, colArr[Xoshiro128pp_nextN(&g_rng, colNum)]);
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            PGO_POP10 ? Connect4_pop10_reset(&profC4, &profP10) : Connect4_reset(&profC4);
        }

        Connect4_destroy(&profC4);
        Connect4_globals_destroy();
        NegaScout_History_destroy();
    }

    FourTheWin_monoTime(&profEnd);
    TransTable_destroy(&NS_table);
    PathTable_destroy(&NS_path);
    //PathStack_destroy(&NS_stack);
    //PathGraph_destroy(&NS_graph);
    printf(FTW_STR_PGO_END, (double)(profEnd.tv_sec - profStart.tv_sec) + (double)(profEnd.tv_nsec - profStart.tv_nsec) / 1e9);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Tunes the NegaScout solver for the Make 7 variant using profile-guided optimization.
/////////////////////////////////////////////////////////////////////////////////////////////////
static inline void NegaScout_Make7_PGO(void)
{
    struct timespec profStart, profEnd;
    Make7 profM7; uint8_t colArr[MAKE7_SIZE_X3], colNum;

    TransTable_init(&NS_table, TT_BASE_SIZE);
    Xoshiro128_init(&g_rng);
    Make7_init(&profM7);
    NegaScout_History_init();
    printf(FTW_STR_PGO_START, FTW_STR_MAKE7, "");
    FourTheWin_monoTime(&profStart);

    constexpr uint8_t TOTAL_ROUNDS = 250;
    constexpr uint8_t HALF_ROUNDS = TOTAL_ROUNDS / 2;

    M7_targetMode = true;
    Make7_setTargetMode();

    bool methodNotDone = true;

    for (uint8_t i = 0; i < TOTAL_ROUNDS; i++)
    {
        if (methodNotDone && i >= HALF_ROUNDS)
        {
            M7_targetMode = methodNotDone = false;
            Make7_setTargetMode();
        }

        while (!Make7_over(&profM7))
        {
            NegaScout_Make7_search(&profM7, 8, -NS_WIN_VAL, NS_WIN_VAL);
            Make7_generate(&profM7, colArr, &colNum);

            const uint8_t PGO_MOVE = colArr[Xoshiro128pp_nextN(&g_rng, colNum)];

            Make7_drop(&profM7, PGO_MOVE >> 3, PGO_MOVE & 7);
        }

        Make7_reset(&profM7);
    }

    FourTheWin_monoTime(&profEnd);
    TransTable_destroy(&NS_table);
    NegaScout_History_destroy();
    printf(FTW_STR_PGO_END, (double)(profEnd.tv_sec - profStart.tv_sec) + (double)(profEnd.tv_nsec - profStart.tv_nsec) / 1e9);
}

#endif

#endif // NEGASCOUT_H //
