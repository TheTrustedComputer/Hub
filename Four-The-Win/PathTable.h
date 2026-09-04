/*
 *  Author: 2026- TheTrustedComputer
 *
 *  A simple, open-address hash table to track positions in the current recursion stack.
 *  This prevents the minimax algorithm from venturing too deeply into cyclic branches.
 *  Full cycle handling is not possible with pure trees; graph algorithms are required.
 */

#ifndef PATHTABLE_H
#define PATHTABLE_H

typedef struct
{
    Board key;
    //uint16_t nodeId, searchId;
    bool edge;
}
PathEntry;

typedef struct
{
    PathEntry *restrict entry;
    uint16_t size;
}
PathTable;

typedef struct
{
    Board *restrict data;
    int16_t cap, top;
}
PathStack;

typedef struct
{
    uint16_t *restrict succ;
    TTLock lock;
    uint32_t cap;
    uint16_t size;
}
PathNode;

typedef struct
{
    PathNode *restrict node;
    uint32_t cap;
    uint16_t size, searchId;
}
PathGraph;

typedef struct
{
    PathGraph *restrict scc;
    uint16_t *restrict idx, *restrict low, *restrict stk;
    bool *restrict onStk;
    uint16_t top, next;
}
PathTarjan;

#ifdef FTW_LIBDIVIDE
#ifdef FTW_BRANCHLESS
    static struct libdivide_u64_branchfree_t libdivide_PathTable_size;
#else
    static struct libdivide_u64_t libdivide_PathTable_size;
#endif
#endif

#ifdef FTW_FASTMOD
#ifdef __SIZEOF_INT128__
    static __uint128_t fastmod_PathTable_size;
#else
    static unsigned _BitInt(128) fastmod_PathTable_size;
#endif
#endif

///////////////////////////////////
/// @brief  Is the key a backedge?
/// @param  _PE
/// @param  _KEY
///////////////////////////////////
static inline bool PathEntry_backedge(const PathEntry *const restrict _PE, const Board _KEY)
{
    return _PE->key == _KEY && _PE->edge;
}

////////////////////////////////////////////
/// @brief  Pushes a key to the path table.
/// @param  _pe
////////////////////////////////////////////
static inline void PathEntry_push(PathEntry *const restrict _pe)
{
    _pe->edge = true;
}

////////////////////////////////////////////
/// @brief  Pops a key from the path table.
/// @param  _pe
////////////////////////////////////////////
static inline void PathEntry_pop(PathEntry *const restrict _pe)
{
    _pe->key = 0;
    _pe->edge = false;
}

///////////////////////////////////////////////////////////
/// @brief  Returns the index to the path table for a key.
/// @param  _PT
/// @param  _KEY
///////////////////////////////////////////////////////////
static inline uint16_t PathTable_index(const PathTable *const restrict _PT, const Board _KEY)
{
#if FTW_C4_MAX_BITS <= 64
#ifdef FTW_FASTMOD
    return fastmod_u64(_KEY, fastmod_PathTable_size, _PT->size);
#elifdef FTW_LIBDIVIDE
#ifdef FTW_BRANCHLESS
    return _KEY - libdivide_u64_branchfree_do(_KEY, &libdivide_PathTable_size) * _PT->size;
#else
    return _KEY - libdivide_u64_do(_KEY, &libdivide_PathTable_size) * _PT->size;
#endif
#else
    return _KEY % _PT->size;
#endif
#else
    return _KEY % _PT->size;
#endif
}

///////////////////////////////////////////////////////////////
/// @brief  Linearly probes the path table for an empty entry.
/// @param  _PT
/// @param  _KEY
///////////////////////////////////////////////////////////////
static inline PathEntry *PathTable_probe(const PathTable *const restrict _PT, const Board _KEY)
{
    PathEntry *restrict pe = &_PT->entry[PathTable_index(_PT, _KEY)];

    while (pe->key && pe->key != _KEY)
    {
        ++pe == &_PT->entry[_PT->size] ? (pe = &_PT->entry[0]) : FTW_VOID_NOP; // end of table
    }

    return pe;
}

////////////////////////////////////////////////////
/// @brief  Initializes the path table with a size.
/// @param  _pt
/// @param  _SIZE
////////////////////////////////////////////////////
static inline void PathTable_init(PathTable *const restrict _pt, const size_t _SIZE)
{
    _pt->entry = REC_calloc((_pt->size = TransTable_size(_SIZE)), sizeof(*_pt->entry), "Could not allocate memory for the path table.", true);

#ifdef FTW_FASTMOD
    fastmod_PathTable_size = computeM_u64(_pt->size);
#elifdef FTW_LIBDIVIDE
#if FTW_BRANCHLESS
    libdivide_PathTable_size = libdivide_u64_branchfree_gen(_pt->size);
#else
    libdivide_PathTable_size = libdivide_u64_gen(_pt->size);
#endif
#endif
}

////////////////////////////////////////////////////////////
/// @brief  Destroys the path table by deallocating memory.
/// @param  _pt
////////////////////////////////////////////////////////////
static inline void PathTable_destroy(PathTable *const restrict _pt)
{
    REC_free(_pt->entry);
}

////////////////////////////////////////
/// @brief  Initializes the path stack.
/// @param  _ps
////////////////////////////////////////
static inline void PathStack_init(PathStack *const restrict _ps)
{
    _ps->top = -1;
    _ps->data = REC_calloc((_ps->cap = 1), sizeof(*_ps->data), "Could not allocate memory for the path stack.", true);
}

////////////////////////////////////////////////////
/// @brief  Releases memory used by the path stack.
/// @param  _ps
////////////////////////////////////////////////////
static inline void PathStack_destroy(PathStack *const restrict _ps)
{
    REC_free(_ps->data);
}

////////////////////////////////////////////
/// @brief  Pushes a key to the path stack.
/// @param  _ps
/// @param  _KEY
////////////////////////////////////////////
static inline void PathStack_push(PathStack *const restrict _ps, const Board _KEY)
{
    ++_ps->top == _ps->cap ? (_ps->data = realloc(_ps->data, (_ps->cap <<= 1) * sizeof(*_ps->data))) : FTW_VOID_NOP;
    _ps->data[_ps->top] = _KEY;
}

//////////////////////////////////////////////
/// @brief  "Pops" a key from the path stack.
/// @param  _ps
/// @note   The top index is decremented.
//////////////////////////////////////////////
static inline void PathStack_pop(PathStack *const restrict _ps)
{
    _ps->top--;
}

///////////////////////////////////////////////////////
/// @brief  Resets the path graph by zeroing its size.
/// @param  _pg
///////////////////////////////////////////////////////
static inline void PathGraph_reset(PathGraph *const restrict _pg)
{
    _pg->size = 0;
}

//////////////////////////////////////////////////////////////
/// @brief  Ages the path graph and increments the search ID.
/// @param  _pg
//////////////////////////////////////////////////////////////
static inline void PathGraph_age(PathGraph *const restrict _pg)
{
    PathGraph_reset(_pg);

    _pg->searchId++;
}

////////////////////////////////////////
/// @brief  Initializes the path graph.
/// @param  _pg
////////////////////////////////////////
static inline void PathGraph_init(PathGraph *const restrict _pg)
{
    PathGraph_reset(_pg);

    _pg->node = REC_calloc((_pg->cap = 1), sizeof(*_pg->node), "Could not allocate memory for the path graph.", true);
}

///////////////////////////////////////////////////
/// @brief  Deallocates edges from the path graph.
/// @param  _pg
///////////////////////////////////////////////////
static inline void PathGraph_destroyEdges(PathGraph *const restrict _pg)
{
    for (uint16_t i = 0; i < _pg->size; i++)
    {
        REC_free(_pg->node[i].succ);
    }
}

///////////////////////////////////////////////
/// @brief  Deallocates the entire path graph.
/// @param  _pg
///////////////////////////////////////////////
static inline void PathGraph_destroy(PathGraph *const restrict _pg)
{
    PathGraph_destroyEdges(_pg);
    REC_free(_pg->node);
}

/////////////////////////////////////////////////////
/// @brief  Gets the node index from the path entry.
/// @param  _pg
/// @param  _pe
/// @param  _LOCK
/////////////////////////////////////////////////////
/*static inline uint16_t PathGraph_getNode(PathGraph *const restrict _pg, PathEntry *const restrict _pe, const TTLock _LOCK)
{
    if (_pg->searchId == _pe->searchId && _pg->node)
    {
        return _pe->nodeId - 1;
    }

    _pg->size == _pg->cap ? (_pg->node = realloc(_pg->node, (_pg->cap <<= 1) * sizeof(*_pg->node))) : FTW_VOID_NOP;

    const uint16_t IDX = _pg->size++;

    _pg->node[IDX] = (PathNode)
    {
        .succ = nullptr,
        .size = 0,
        .cap = 0,
        .lock = _pe->key ^ _LOCK
    };

    _pe->nodeId = IDX + 1;
    _pe->searchId = _pg->searchId;

    return IDX;
}*/

////////////////////////////////////////////
/// @brief  Adds an edge to the path graph.
/// @param  _pg
/// @param  _PT
/// @param  _pe
/// @param  _U_KEY  The "from" key.
/// @param  _LOCK   The "to" lock.
////////////////////////////////////////////
/*static inline void PathGraph_addEdge(PathGraph *const restrict _pg, const PathTable *const restrict _PT, PathEntry *const restrict _pe, const Board _U_KEY, const TTLock _LOCK)
{
    const uint16_t U = PathGraph_getNode(_pg, PathTable_probe(_PT, _U_KEY), _LOCK);
    const uint16_t V = PathGraph_getNode(_pg, _pe, _LOCK);

    PathNode *const restrict pn = &_pg->node[U];

    pn->size == pn->cap ? (pn->succ = realloc(pn->succ, (pn->cap = pn->cap ? pn->cap << 1 : 1) * sizeof(*pn->succ))) : FTW_VOID_NOP;
    pn->succ[pn->size++] = V;
}*/

////////////////////////////////////////////////////////////
/// @brief  Initializes Tarjan's algorithm data structures.
/// @param  _pj
/// @param  _pg
////////////////////////////////////////////////////////////
static inline void PathTarjan_init(PathTarjan *const restrict _pj, PathGraph *const restrict _pg)
{
    _pj->scc = _pg;
    _pj->top = 0;
    _pj->next = 1;
    _pj->idx = calloc(_pg->size, sizeof(*_pj->idx));
    _pj->low = calloc(_pg->size, sizeof(*_pj->low));
    _pj->stk = calloc(_pg->size, sizeof(*_pj->stk));
    _pj->onStk = calloc(_pg->size, sizeof(*_pj->onStk));
}

////////////////////////////////////////////////////////////
/// @brief  Deletes memory reserved for Tarjan's algorithm.
/// @param  _pj
////////////////////////////////////////////////////////////
static inline void PathTarjan_destroy(PathTarjan *const restrict _pj)
{
    free(_pj->idx);
    free(_pj->low);
    free(_pj->stk);
    free(_pj->onStk);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Main Tarjan's algorithm implementation to find strongly connected components and promote transposition table entries to exact draws.
/// @param  _pj
/// @param  _TT
/// @param  _V
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void PathTarjan_findSCCs(PathTarjan *const restrict _pj, const TransTable *const restrict _TT, const uint16_t _V)
{
    _pj->idx[_V] = _pj->next;
    _pj->low[_V] = _pj->next++;
    _pj->stk[_pj->top++] = _V;
    _pj->onStk[_V] = true;

    const PathGraph *const restrict SCC = _pj->scc;

    PathNode *restrict sccNode = &SCC->node[_V];
    uint16_t i, w;

    for (i = 0; i < sccNode->size; i++)
    {
        w = sccNode->succ[i];
        !_pj->idx[w] ? PathTarjan_findSCCs(_pj, _TT, w) : FTW_VOID_NOP;
        _pj->onStk[w] && _pj->low[w] < _pj->low[_V] ? (_pj->low[_V] = _pj->low[w]) : FTW_VOID_NOP;
    }

    if (_pj->low[_V] == _pj->idx[_V])
    {
        uint16_t *restrict sccIds = nullptr, compCnt = 0;
        uint32_t compCap = 0;

        do
        {
            w = _pj->stk[--_pj->top];
            _pj->onStk[w] = false;
            compCnt == compCap ? (sccIds = realloc(sccIds, (compCap = compCap ? compCap << 1 : 1) * sizeof(*sccIds))) : FTW_VOID_NOP;
            sccIds[compCnt++] = w;
        }
        while (w != _V);

        if (compCnt > 1)
        {
            for (i = 0; i < compCnt; i++)
            {
                sccNode = &SCC->node[sccIds[i]];

                TransBucket *const restrict sccTB = &_TT->bucket[TransTable_index(_TT, sccNode->lock)];
                bool inexactDraw = true; TransEntry *restrict sccTE; uint8_t j;

                for (j = 0; j < 4; j++)
                {
                    sccTE = &sccTB->entry[j];

                    if (!sccTE->lock)
                    {
                        sccTE->lock = sccNode->lock;
                        goto PathTarjan_findSCCs_lockHit;
                    }

                    if (sccTE->lock == sccNode->lock)
                    {
                        goto PathTarjan_findSCCs_lockHit;
                    }

                    continue;

                PathTarjan_findSCCs_lockHit:
                    sccTE->value = 0; // Draw value
                    sccTE->depth = UINT16_MAX;
                    sccTE->bound = TT_EXACT;
                    inexactDraw = false;
                    break;
                }

                for (j = 0; inexactDraw && j < 4; j++)
                {
                    sccTE = &sccTB->entry[j];

                    if (sccTE->bound != TT_EXACT)
                    {
                        sccTE->lock = sccNode->lock;
                        sccTE->value = 0;
                        sccTE->depth = UINT16_MAX;
                        sccTE->bound = TT_EXACT;
                        break;
                    }
                }
            }
        }

        free(sccIds);
    }
}

////////////////////////////////////////////////////
/// @brief  The entry point for Tarjan's algorithm.
/// @param  _pg
/// @param  _tt
////////////////////////////////////////////////////
static inline void PathGraph_Tarjan(PathGraph *const restrict _pg, TransTable *const restrict _tt)
{
    PathTarjan tarjan;

    PathTarjan_init(&tarjan, _pg);

    for (uint16_t i = 0; i < _pg->size; i++)
    {
        !tarjan.idx[i] ? PathTarjan_findSCCs(&tarjan, _tt, i) : FTW_VOID_NOP;
    }

    PathTarjan_destroy(&tarjan);
}

#endif // PATHTABLE_H //
