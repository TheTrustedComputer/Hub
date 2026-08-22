/*
 *  Author: 2026- TheTrustedComputer
 *
 *  The transposition table is a hash map that stores previously examined nodes in a game tree.
 *  In many complex games, multiple move sequences can lead to the same board position.
 *  Caching these states allows the game-playing engine or solver to avoid redundant work.
 *
 *  While optional, we also record moves in the table to enable dynamic move ordering.
 *  Alpha-beta pruning is most effective when it considers the best moves first.
 *  This technique has been shown to further reduce the node count than without it.
 *  With the help of game-specific ordering, the two methods reinforce each other.
 */

#ifndef TRANSTABLE_H
#define TRANSTABLE_H

static constexpr uint32_t TT_BASE_SIZE = 8388608;

typedef enum : uint8_t
{
    TT_LOWER,
    TT_EXACT,
    TT_UPPER
}
TransBound;

typedef enum : uint8_t
{
    TT_MISS,
    TT_HIT,
    TT_CUT
}
TransProbe;

#pragma pack(push, 1)

typedef struct
{
    uint64_t lock;
    int16_t value;
    uint16_t depth;
    uint8_t move; // bitwise NOT of move
    TransBound bound;
}
TransEntry;

typedef struct
{
    TransEntry entry[4];
}
TransBucket;

typedef struct
{
    TransBucket *restrict bucket;
    uint32_t size; // actual size
}
TransTable;

#pragma pack(pop)

#ifdef FTW_LIBDIVIDE
#ifdef FTW_BRANCHLESS
    static struct libdivide_u64_branchfree_t libdivide_TransTable_size;
#else
    static struct libdivide_u64_t libdivide_TransTable_size;
#endif
#endif

#ifdef FTW_FASTMOD
#ifdef __SIZEOF_INT128__
    static __uint128_t fastmod_TransTable_size;
#else
    static unsigned _BitInt(128) fastmod_TransTable_size;
#endif
#endif

#if defined(__unix__) || defined(_WIN64)
    #define TransTable_reset(_tt) TransTable_freeClear(_tt)
#else
    #define TransTable_reset(_tt) TransTable_memsetClear(_tt)
#endif

///////////////////////////////////////////////////
/// @brief  Finds whether `_N` is prime (1 * _N).
/// @param  _N
/// @return `true` if prime; `false` if composite.
///////////////////////////////////////////////////
static inline bool TransTable_prime(const uint32_t _N)
{
    if (_N <= 1)
    {
        return false;
    }

    if (_N <= 3)
    {
        return true;
    }

    if (!((_N % 2) && (_N % 3)))
    {
        return false;
    }

    for (size_t p = 5; p * p <= _N; p += 6)
    {
        if (!((_N % p) && (_N % (p + 2))))
        {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////
/// @brief  Searches for the largest prime <= `_n`.
/// @param  _n
////////////////////////////////////////////////////
static inline uint32_t TransTable_size(size_t _n)
{
    if (_n > UINT32_MAX)
    {
        _n = UINT32_MAX;
    }

    if (!(_n % 2))
    {
        _n--;
    }

    while (!TransTable_prime(_n))
    {
        _n -= 2;
    }

    return _n;
}

//////////////////////////////////////////////////////////////////
/// @brief          Computes the hash index for a given lock.
/// @param  _TT     Unaliased pointer to the transposition table.
/// @param  _LOCK   Unique integer representation of a state.
//////////////////////////////////////////////////////////////////
static inline uint32_t TransTable_index(const TransTable *const restrict _TT, const uint64_t _LOCK)
{
#if FTW_C4_MAX_BITS <= 64
#ifdef FTW_FASTMOD
    return fastmod_u64(_LOCK, fastmod_TransTable_size, _TT->size);
#elifdef FTW_LIBDIVIDE
#ifdef FTW_BRANCHLESS
    return _LOCK - libdivide_u64_branchfree_do(_LOCK, &libdivide_TransTable_size) * _TT->size;
#else
    return _LOCK - libdivide_u64_do(_LOCK, &libdivide_TransTable_size) * _TT->size;
#endif
#else
    return _LOCK % _TT->size;
#endif
#else
    return _LOCK % _TT->size;
#endif
}

////////////////////////////////////////////////////////////////
/// @brief          Probes an entry in the transposition table.
/// @param  _TB     Unaliased pointer to the table bucket.
/// @param  _LOCK   Unique integer representation of a state.
/// @param  _DEP    Current search depth of the node.
/// @param  _A      Alpha bound at end of search.
/// @param  _B      Beta bound at end of search.
/// @param  _val    Intermediate minimax value.
/// @param  _move   The move that had led to the cutoff.
/// @return         `TT_CUT` if found and proven;
///                 `TT_HIT` if found but cannot be proven;
///                 `TT_MISS` if not found.
////////////////////////////////////////////////////////////////
static inline TransProbe TransBucket_probe
(
    const TransBucket *const restrict _TB,
    const uint64_t _LOCK,
    const uint16_t _DEP,
    const int16_t _A,
    const int16_t _B,
    int16_t *const restrict _val,
    uint8_t *const restrict _move
)
{
    *_move = 0;

    for (uint8_t i = 0; i < 4; i++)
    {
        const TransEntry *const restrict TE = &_TB->entry[i];

        if (TE->lock == _LOCK)
        {
            *_move = TE->move;

            const int16_t TE_VALUE = TE->value;

            if (TE->depth >= _DEP)
            {
                switch (TE->bound)
                {
                case TT_LOWER:
                    if (TE_VALUE >= _B)
                    {
                        goto TransTable_proven;
                    }
                    break;
                case TT_EXACT:
                    goto TransTable_proven;
                case TT_UPPER:
                    if (TE_VALUE <= _A)
                    {
                        goto TransTable_proven;
                    }
                    break;
                }
            }

            return TT_HIT;

        TransTable_proven:
            *_val = TE_VALUE;

            return TT_CUT;
        }
    }

    return TT_MISS;
}

//////////////////////////////////////////////////////////////////
/// @brief          Stores an entry into the transposition table.
/// @param  _tb     Unaliased pointer to the table bucket.
/// @param  _LOCK   Unique integer representation of a state.
/// @param  _VAL    Intermediate minimax value.
/// @param  _DEP    Current search depth of the node.
/// @param  _A      Old alpha bound.
/// @param  _B      New beta bound.
/// @param  _MOVE   Move that forced the cutoff.
//////////////////////////////////////////////////////////////////
static inline void TransBucket_store
(
    TransBucket *const restrict _tb,
    const uint64_t _LOCK,
    const int16_t _VAL,
    const uint16_t _DEP,
    const int16_t _A,
    const int16_t _B,
    const uint8_t _MOVE
)
{
    TransBound bound = _VAL <= _A ? TT_UPPER : _VAL >= _B ? TT_LOWER : TT_EXACT;
    TransEntry *restrict te; uint8_t i;

    for (i = 0; i < 4; i++)
    {
        te = &_tb->entry[i];

        if (!te->lock)
        {
            goto TransTable_new;
        }

        if (te->lock == _LOCK && (te->depth < _DEP || (te->depth == _DEP && bound == te->bound)))
        {
            goto TransTable_update;
        }
    }

    // Replacement scheme -- evict the entry with the lowest depth

    te = &_tb->entry[0];

    uint16_t replaceGrade = te->depth;
    uint8_t replaceBucket = 0;

    for (i = 1; i < 4; i++)
    {
        te = &_tb->entry[i];

        const uint16_t GRADE = te->depth;

        GRADE < replaceGrade ? replaceGrade = GRADE, replaceBucket = i : FTW_VOID_NOP;
    }

    te = &_tb->entry[replaceBucket];

TransTable_new:
    te->lock = _LOCK;
TransTable_update:
    te->value = _VAL;
    te->depth = _DEP;
    te->move = ~_MOVE;
    te->bound = bound;
}

/////////////////////////////////////////////////////////////
/// @brief  Initializes the transposition table with a size.
/// @param  _tt
/// @param  _SIZE
/////////////////////////////////////////////////////////////
static inline void TransTable_init(TransTable *const restrict _tt, const size_t _SIZE)
{
    _tt->bucket = REC_calloc((_tt->size = TransTable_size(_SIZE)), sizeof(*_tt->bucket), "Could not allocate memory for the transposition table.", true);

#ifdef FTW_FASTMOD
    fastmod_TransTable_size = computeM_u64(_tt->size);
#elifdef FTW_LIBDIVIDE
#if FTW_BRANCHLESS
    libdivide_TransTable_size = libdivide_u64_branchfree_gen(_tt->size);
#else
    libdivide_TransTable_size = libdivide_u64_gen(_tt->size);
#endif
#endif
}

//////////////////////////////////////////////////////////////////
/// @brief  Destroys the transposition table by releasing memory.
/// @param  _tt
//////////////////////////////////////////////////////////////////
static inline void TransTable_destroy(TransTable *const restrict _tt)
{
    REC_free(_tt->bucket);
}

///////////////////////////////////////////////////////////
/// @brief  Clears the transposition table via `memset()`.
/// @param  _tt
///////////////////////////////////////////////////////////
static inline void TransTable_memsetClear(TransTable *const restrict _tt)
{
    memset(_tt->bucket, 0, _tt->size * sizeof(*_tt->bucket));
}

/////////////////////////////////////////////////////////////
/// @brief  Frees and reinitializes the transposition table.
/// @param  _tt
/////////////////////////////////////////////////////////////
static inline void TransTable_freeClear(TransTable *const restrict _tt)
{
    TransTable_destroy(_tt);
    TransTable_init(_tt, _tt->size);
}

#endif // TRANSTABLE_H //
