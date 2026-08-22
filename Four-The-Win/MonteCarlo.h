/*
 *  Author: 2026- TheTrustedComputer
 *
 *  Monte Carlo Tree Search is an algorithm that samples the game tree for the best move, running in four phases:
 *  1) Selection - choose a node with the most promising wdl.
 *  2) Expansion - create the node(s) if it has not been visited.
 *  3) Simulation - complete a game round from this node.
 *  4) Backpropagation - update node values based on the wdl.
 *
 *  Unlike the minimax family of algorithms, it does not require domain knowledge or heuristics.
 *  The algorithm learns through simulated win/loss statistics and informed decision-making.
 *
 *  This method is a hybrid of MCTS and Proof-Number Search with OR-node evaluation for win/draw/loss backpropagation.
 *  Instead of win rates, the algorithm employs accumulated weighted rewards to promote faster wins and slower losses.
 *  Rollouts are purely random to leverage uniform exploration of nodes, using traditional UCT to guide future searches.
 *  Non-terminals reward wins +1, draws 0, and losses -1; terminals reward thrice the value, checked during expansion.
 *  Once a node is fully solved, its subtree is pruned to focus on uncertain branches of the tree, a la alpha-beta.
 *  To accelerate convergence to the minimax value, we employed a hash table to halt the simulation at resolved nodes.
 *
 *  Testing demonstrated that it generates identical solutions and converges faster to minimax than standard Monte Carlo.
 *  The combination avoids the weaknesses of both: exploding memory on expensive proofs and susceptibility to tactics.
 *  This blend of techniques emerged from a disappointing initial MCTS implementation on a previous build of a game solver.
 */

#ifndef MONTECARLO_H
#define MONTECARLO_H

typedef enum : uint8_t
{
    MCTS_NONE,
    MCTS_LOSS,
    MCTS_DRAW,
    MCTS_WIN
}
MCTSWDL;

#pragma pack(push, 1)

typedef struct MCTSNode
{
    struct MCTSNode *restrict ancestor;
    struct MCTSNode *restrict *restrict descendants;
    Board key; double visits, score;
    uint8_t move, count, nones, index;
    MCTSWDL wdl;
}
MCTSNode;

typedef struct MCTSEntry
{
    struct MCTSEntry *restrict next;
    Board key;
    uint8_t exp; // expansion count
    MCTSWDL wdl;
}
MCTSEntry;

typedef struct MCTSTable
{
    struct MCTSEntry *restrict *restrict bucket;
    uint32_t size;
}
MCTSTable;

typedef struct MCTSResult
{
    MCTSNode *restrict root;
    const MCTSWDL *restrict wdl;
    const unsigned long long *restrict trials;
    unsigned long long *restrict secs;
    double reward; uint8_t move;
}
MCTSResult;

#pragma pack(pop)

static MemoryPool MCTS_nodePool, MCTS_entryPool;
static unsigned long long MCTS_trials;
static MCTSTable MCTS_table;
static MCTSNode *restrict *restrict MCTS_termNodes;
static uint8_t *restrict MCTS_movArr, MCTS_termCnt;
static atomic_bool MCTS_run;

static bool (*MCTSNode_Connect4_evaluate)(MCTSNode *const restrict, const Connect4 *const restrict);
static double (*MCTSNode_Connect4_simulate)(Connect4 *const restrict);
static void (*MCTSResult_print)(const MCTSResult *const restrict);

//////////////////////////////////////////////////////
/// @brief  Returns the index of a node in the table.
/// @param  _MCT
/// @param  _KEY
//////////////////////////////////////////////////////
static inline uint32_t MCTSTable_index(const MCTSTable *const restrict _MCT, const Board _KEY)
{
    return _KEY % _MCT->size;
}

////////////////////////////////////////////////////////////
/// @brief  Finds a node in the table by key.
/// @param  _MCT
/// @param  _KEY
/// @return Pointer to the node, or `nullptr` if not found.
////////////////////////////////////////////////////////////
static inline MCTSEntry *MCTSTable_find(const MCTSTable *const restrict _MCT, const Board _KEY)
{
    for (MCTSEntry *restrict me = _MCT->bucket[MCTSTable_index(_MCT, _KEY)]; me; me = me->next)
    {
        if (me->key == _KEY)
        {
            return me;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////
/// @brief  Increases the expansion count for a node.
/// @param  _MCT
/// @param  _KEY
/// @note   Remains constant on trees; changes on graphs.
//////////////////////////////////////////////////////////
static inline void MCTSTable_increaseExp(const MCTSTable *const restrict _MCT, const Board _KEY)
{
    MCTSEntry *const restrict me = MCTSTable_find(_MCT, _KEY);

    if (me)
    {
        me->exp++;
    }
}

/////////////////////////////////////////////////////////////////////
/// @brief  Inserts a WDL score into the table by a key of the node.
/// @param  _MCT
/// @param  _KEY
/// @param  _WDL
/////////////////////////////////////////////////////////////////////
static inline void MCTSTable_insert(const MCTSTable *const restrict _MCT, const Board _KEY, const MCTSWDL _WDL)
{
    MCTSEntry *const restrict me = MCTSTable_find(_MCT, _KEY);

    if (!me)
    {
        const uint32_t IDX = MCTSTable_index(_MCT, _KEY);

        MCTSEntry *const restrict newEntry = MemoryPool_alloc(&MCTS_entryPool, sizeof(*newEntry));

        newEntry->key = _KEY;
        newEntry->exp = 0;
        newEntry->wdl = _WDL;
        newEntry->next = _MCT->bucket[IDX];

        _MCT->bucket[IDX] = newEntry;
    }
}

/////////////////////////////////////////////////////////////////
/// @brief  Lookups the WDL score for a node's key in the table.
/// @param  _MCT
/// @param  _KEY
/////////////////////////////////////////////////////////////////
static inline MCTSWDL MCTSTable_WDL(const MCTSTable *const restrict _MCT, const Board _KEY)
{
    MCTSEntry *const restrict me = MCTSTable_find(_MCT, _KEY);

    return me && me->key == _KEY ? me->wdl : MCTS_NONE;
}

/////////////////////////////////////////////////////////////////
/// @brief          Initializes the table with a specified size.
/// @param _mt      Unaliased pointer to the table.
/// @param _SIZE    Requested size of the table.
/////////////////////////////////////////////////////////////////
static inline void MCTSTable_init(MCTSTable *const restrict _mt, const uint32_t _SIZE)
{
    _mt->bucket = REC_calloc((_mt->size = _SIZE), sizeof(*_mt->bucket), "Could not allocate memory for the MCTS table.", true);
}

//////////////////////////////////////////////////////
/// @brief      Destroys the table by freeing memory.
/// @param _mt  Unaliased pointer to the table.
//////////////////////////////////////////////////////
static inline void MCTSTable_destroy(MCTSTable *const restrict _mt)
{
    REC_free(_mt->bucket);
}

////////////////////////////////////////////////////////
/// @brief  Evaluates a Connect 4 node for termination.
/// @param  _node
/// @param  _C4
////////////////////////////////////////////////////////
static inline bool MCTSNode_Connect4_original_evaluate(MCTSNode *const restrict _node, const Connect4 *const restrict _C4)
{
    if (Connect4_fourInARow(_C4->side ^ _C4->mask))
    {
        _node->wdl = MCTS_LOSS;

        return true;
    }

    if (Connect4_full(_C4))
    {
        _node->wdl = MCTS_DRAW;

        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////
/// @brief  Evaluates a Connect 4 Misere node for termination.
/// @param  _node
/// @param  _C4
///////////////////////////////////////////////////////////////
static inline bool MCTSNode_Connect4_misere_evaluate(MCTSNode *const restrict _node, const Connect4 *const restrict _C4)
{
    if (Connect4_fourInARow(_C4->side ^ _C4->mask))
    {
        _node->wdl = MCTS_WIN;

        return true;
    }

    if (Connect4_full(_C4))
    {
        _node->wdl = MCTS_DRAW;

        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////
/// @brief  Evaluates a Connect 4 PopOut node for termination.
/// @param  _node
/// @param  _C4
///////////////////////////////////////////////////////////////
static inline bool MCTSNode_Connect4_popout_evaluate(MCTSNode *const restrict _node, const Connect4 *const restrict _C4)
{
    if (Connect4_fourInARow(_C4->side ^ _C4->mask))
    {
        _node->wdl = MCTS_LOSS;

        return true;
    }

    if (Connect4_fourInARow(_C4->side))
    {
        _node->wdl = MCTS_WIN;

        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////
/// @brief  Calculates the Upper Confidence Bound of a node.
/// @param  _NODE
/////////////////////////////////////////////////////////////
static inline double MCTSNode_UCB1(const MCTSNode *const restrict _NODE)
{
    return _NODE->score / _NODE->visits + sqrt(2.0 * log(_NODE->ancestor->visits) / _NODE->visits);
}

///////////////////////////////////////////////////////////////
/// @brief          Initializes a node with meaningful values.
/// @param _node    Unaliased pointer to the node.
/// @param _ancest  Unaliased pointer to the ancestor.
/// @param _INDEX   Index from the ancestor node.
/// @param _MOVE    A move that led to this node.
///////////////////////////////////////////////////////////////
static inline void MCTSNode_init(MCTSNode *const restrict _node, MCTSNode *const restrict _ancest, const uint8_t _INDEX, const uint8_t _MOVE)
{
    _node->ancestor = _ancest;
    _node->index = _INDEX;
    _node->move = _MOVE;
    _node->descendants = nullptr;
    _node->visits = _node->score = 0.0;
    _node->count = _node->nones = 0;
    _node->wdl = MCTS_NONE;
}

/////////////////////////////////////////////////
/// @brief  Destroys every descendant of a node.
/// @param  _node
/////////////////////////////////////////////////
static inline void MCTSNode_destroy(MCTSNode *const restrict _node)
{
    if (_node->descendants)
    {
        for (uint8_t i = 0; i < _node->count; i++)
        {
            MCTSNode_destroy(_node->descendants[i]);
        }

        _node->descendants = nullptr;
    }
}

/////////////////////////////////////////////////
/// @brief  Prune all sub-descendants of a node.
/// @param  _node
/////////////////////////////////////////////////
static inline void MCTSNode_prune(MCTSNode *const restrict _node)
{
    for (uint8_t i = 0; i < _node->count; i++)
    {
        MCTSNode *const restrict subnode = _node->descendants[i];

        MCTSNode_destroy(subnode);

        subnode->descendants = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////
/// @brief  Swaps a proven node to the bottom of its ancestor's list.
/// @param  _ancest
/// @param  _descend
//////////////////////////////////////////////////////////////////////
static inline void MCTSNode_swap(MCTSNode *const restrict _ancest, MCTSNode *const restrict _descend)
{
    const uint8_t INDEX = _descend->index;
    const uint8_t LAST = --_ancest->nones;

    if (INDEX < LAST)
    {
        MCTSNode *const restrict swapper = _ancest->descendants[LAST];

        _ancest->descendants[LAST] = _descend;
        _ancest->descendants[INDEX] = swapper;
        swapper->index = INDEX;
        _descend->index = LAST;
    }
}

//////////////////////////////////////////////////////////////
/// @brief  Proves a node's WDL outcome from its descendants.
/// @param  _node
/// @return `true` if proved; otherwise `false`.
//////////////////////////////////////////////////////////////
static inline bool MCTSNode_prove(MCTSNode *const restrict _node)
{
    uint8_t wins = 0, nones = 0;
    bool draws = false;

    for (uint8_t i = 0; i < _node->count; i++)
    {
        switch (_node->descendants[i]->wdl)
        {
        case MCTS_LOSS:
            _node->wdl = MCTS_WIN;
            MCTSNode_prune(_node);
            return true;
        case MCTS_DRAW:
            draws = true;
            break;
        case MCTS_WIN:
            wins++;
            break;
        default:
            nones++;
            break;
        }
    }

    if (!nones)
    {
        if (draws)
        {
            _node->wdl = MCTS_DRAW;
            MCTSNode_prune(_node);
            return true;
        }
        else if (wins && wins == _node->count)
        {
            _node->wdl = MCTS_LOSS;
            MCTSNode_prune(_node);
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////
/// @brief       MCTS selection phase for Connect 4.
/// @param _node The node to select from the root.
/// @param _c4   Unaliased pointer to the game state.
/// @return      Highest UCB1 descendant.
//////////////////////////////////////////////////////
static inline MCTSNode *MCTSNode_Connect4_select(MCTSNode *restrict _node, Connect4 *const restrict _c4)
{
    while (_node->descendants)
    {
        MCTSNode *restrict selected = nullptr;
        double currUCB1, bestUCB1 = -DBL_MAX;

        for (uint8_t i = 0; i < _node->nones; i++)
        {
            MCTSNode *restrict candidate = _node->descendants[i];

            if (!candidate->visits)
            {
                return _node;
            }

            if ((currUCB1 = MCTSNode_UCB1(candidate)) > bestUCB1)
            {
                bestUCB1 = currUCB1;
                selected = candidate;
            }
        }

        assert(selected); // TODO: Support PopOut

        Connect4_play(_c4, selected->move);

        _node = selected;
    }

    return _node;
}

//////////////////////////////////////////////////////
/// @brief       MCTS expansion phase for Connect 4.
/// @param _node The node that will be expanded.
/// @param _c4   Unaliased pointer to the game state.
/// @return      A randomly picked descendant node.
//////////////////////////////////////////////////////
static inline MCTSNode *MCTSNode_Connect4_expand(MCTSNode *restrict _node, Connect4 *const restrict _c4)
{
    if (!_node->descendants)
    {
        Connect4_generate(_c4, MCTS_movArr, &_node->count);

        _node->descendants = MemoryPool_alloc(&MCTS_nodePool, sizeof(*_node->descendants) * _node->count);
        MCTS_termCnt = 0;

        for (uint8_t i = 0; i < _node->count; i++)
        {
            _node->nones++;

            MCTSNode *restrict *restrict newNode = &_node->descendants[i];

            *newNode = MemoryPool_alloc(&MCTS_nodePool, sizeof(**newNode));
            MCTSNode_init(*newNode, _node, i, MCTS_movArr[i]);
            Connect4_play(_c4, (*newNode)->move);
            (*newNode)->key = Connect4_key(_c4);

            if (MCTSNode_Connect4_evaluate(*newNode, _c4))
            {
                MCTS_termNodes[MCTS_termCnt++] = *newNode;
                MCTSNode_swap(_node, *newNode);
                MCTSTable_insert(&MCTS_table, (*newNode)->key, (*newNode)->wdl);
            }

            Connect4_unplay(_c4);
        }

        MCTSNode *const restrict tNode = MCTS_termCnt ? MCTS_termNodes[Xoshiro128pp_nextN(&g_rng, MCTS_termCnt)] : nullptr;

        if (tNode && tNode->wdl == MCTS_LOSS)
        {
            return tNode;
        }
    }

    return _node->descendants[Xoshiro128pp_nextN(&g_rng, _node->count)];
}

/////////////////////////////////////////////////////////
/// @brief  MCTS simulation/rollout phase for Connect 4.
/// @param  _c4
/// @return A reward for the player who initiated it.
/////////////////////////////////////////////////////////
static inline double MCTSNode_Connect4_original_simulate(Connect4 *const restrict _c4)
{
    const bool SIM_TURN = _c4->plies & 1;

    uint8_t simCnt; MCTSWDL simWDL;

    for (;;)
    {
        if ((simWDL = MCTSTable_WDL(&MCTS_table, Connect4_key(_c4))) != MCTS_NONE)
        {
            const double SCORE = simWDL == MCTS_WIN ? 2.0 : simWDL == MCTS_LOSS ? -2.0 : 0.0;

            return SIM_TURN == (_c4->plies & 1) ? -SCORE : SCORE;
        }

        Connect4_generate(_c4, MCTS_movArr, &simCnt);

        if (simCnt)
        {
            Connect4_play(_c4, MCTS_movArr[Xoshiro128pp_nextN(&g_rng, simCnt)]);

            if (Connect4_fourInARow(_c4->side ^ _c4->mask))
            {
                switch (C4_variant)
                {
                default:
                    return SIM_TURN == (_c4->plies & 1) ? 1.0 : -1.0;
                case CONNECT4_MISERE:
                    return SIM_TURN == (_c4->plies & 1) ? -1.0 : 1.0;
                }
            }
        }
        else
        {
            return 0.0;
        }
    }
}

///////////////////////////////////////////////////////////////////
/// @brief          Monte Carlo Tree Search backpropagation phase.
/// @param _node    Leaf node.
/// @param _score   Simulation score.
///////////////////////////////////////////////////////////////////
static inline void MCTSNode_backpropagate(MCTSNode *restrict _node, double _score)
{
    while (_node)
    {
        _node->visits++;
        _node->score += _score;

        MCTSNode *const restrict parent = _node->ancestor;

        if (MCTSNode_prove(_node) && parent)
        {
            MCTSTable_insert(&MCTS_table, _node->key, _node->wdl);
            MCTSNode_swap(parent, _node);
        }

        _score = -_score;
        _node = parent;
    }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief          Chooses the most visited node from a list of descendants.
/// @param _node    Root node.
//////////////////////////////////////////////////////////////////////////////
static inline MCTSNode *MCTSNode_mostRobust(MCTSNode *const restrict _node)
{
    uint8_t i, draws = 0, losses = 0, nones = 0;

    for (i = 0; !(losses && nones) && i < _node->count; i++)
    {
        switch (_node->descendants[i]->wdl)
        {
        case MCTS_LOSS:
            losses++;
            break;
        case MCTS_DRAW:
            draws++;
            break;
        default:
            nones++;
            break;
        }
    }

    MCTSWDL tier;

    if (losses)
    {
        tier = MCTS_LOSS;
    }
    else if (nones)
    {
        tier = MCTS_NONE;
    }
    else
    {
        tier = draws ? MCTS_DRAW : MCTS_WIN;
    }

    MCTSNode *restrict robust = _node->descendants[0];

    switch (tier)
    {
    case MCTS_LOSS:
    {
        MCTSNode *restrict lostNodes[_node->count];
        uint8_t lostCnt = 0;

        for (i = 0; i < _node->count; i++)
        {
            if (_node->descendants[i]->wdl == tier)
            {
                lostNodes[lostCnt++] = _node->descendants[i];
            }
        }

        return lostNodes[Xoshiro128pp_nextN(&g_rng, lostCnt)];
    }
    default:
        for (i = 1; i < _node->count; i++)
        {
            if (_node->descendants[i]->visits > robust->visits)
            {
                robust = _node->descendants[i];
            }
        }
        break;
    }

    return robust;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief  Prints the running status of Monte Carlo Tree Search on Connect 4.
/// @param  _RES
///////////////////////////////////////////////////////////////////////////////
static inline void MCTSResult_Connect4_print(const MCTSResult *const restrict _RES)
{
    const unsigned long long SPEED = *_RES->trials / *_RES->secs;

    uint8_t move = _RES->move;

    const char M_CHAR = move + '1';

    switch (*_RES->wdl)
    {
    case MCTS_WIN:
        printf("\r\e[1;92m%c %s\e[0m %llu %llu %llu        ", M_CHAR, FTW_STR_WIN, *_RES->trials, SPEED, *_RES->secs);
        break;
    case MCTS_DRAW:
        printf("\r\e[1;93m%c %s\e[0m %llu %llu %llu        ", M_CHAR, FTW_STR_DRAW, *_RES->trials, SPEED, *_RES->secs);
        break;
    case MCTS_LOSS:
        printf("\r\e[1;91m%c %s\e[0m %llu %llu %llu        ", M_CHAR, FTW_STR_LOSS, *_RES->trials, SPEED, *_RES->secs);
        break;
    default:
        printf("\r\e[1m%c\e[0m %.3f %llu %llu %llu ", M_CHAR, _RES->reward, *_RES->trials, SPEED, *_RES->secs);
        fflush(stdout);
        break;
    }
}

/////////////////////////////////////////////////////////////
/// @brief      The progress monitoring thread for MCTS.
/// @param _arg Unaliased pointer to an `MCTSResult` struct.
/// @return     Always zero (successful exit).
/////////////////////////////////////////////////////////////
static inline int MCTSResult_thread(void *const restrict _arg)
{
    const struct timespec _1_SEC = {.tv_sec = 1, .tv_nsec = 0};

    MCTSResult *const restrict res = _arg;

    while (atomic_load_explicit(&MCTS_run, memory_order_relaxed))
    {
        thrd_sleep(&_1_SEC, nullptr);

        if (atomic_load_explicit(&MCTS_run, memory_order_relaxed) && *res->wdl == MCTS_NONE)
        {
            MCTSNode *const restrict currNode = MCTSNode_mostRobust(res->root);

            res->move = currNode->move;
            res->reward = -res->root->score / res->root->visits;
            (*res->secs)++;

            MCTSResult_print(res);
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////
/// @brief  Initializes the MCTS game-specific function pointers.
//////////////////////////////////////////////////////////////////
static inline void MCTS_funtPtrs_init(void)
{
    MCTSNode_Connect4_evaluate = MCTSNode_Connect4_original_evaluate;
    MCTSNode_Connect4_simulate = MCTSNode_Connect4_original_simulate;
    MCTSResult_print = MCTSResult_Connect4_print;

    switch (C4_variant)
    {
    case CONNECT4_MISERE:
        MCTSNode_Connect4_evaluate = MCTSNode_Connect4_misere_evaluate;
        break;
    case CONNECT4_POPOUT:
        MCTSNode_Connect4_evaluate = MCTSNode_Connect4_popout_evaluate;
    default:
        break;
    }
}

///////////////////////////////////////////////////////////////
/// @brief  Interrupt handler to stop Monte Carlo Tree Search.
/// @param  _UNUSED
///////////////////////////////////////////////////////////////
static inline void MCTS_stopSearch(const int _UNUSED)
{
    (void)(_UNUSED);

    atomic_store_explicit(&MCTS_run, false, memory_order_relaxed);
}

/////////////////////////////////////////////////////////////
/// @brief  Begins a Monte Carlo Tree Search on Connect 4.
/// @param  _C4
/// @return Game outcome (`WIN`, `DRAW`, `LOSS`, or `NONE`).
/////////////////////////////////////////////////////////////
static inline MCTSWDL MCTS_Connect4_search(const Connect4 *const restrict _C4)
{
    MCTSNode root;
    MCTSNode_init(&root, nullptr, 0, 0);

    root.key = Connect4_key(_C4);

    MCTSTable_init(&MCTS_table, NS_table.size);
    MemoryPool_init(&MCTS_nodePool);
    MemoryPool_init(&MCTS_entryPool);
    atomic_init(&MCTS_run, true);
    signal(SIGINT, MCTS_stopSearch);

    MCTS_termNodes = REC_calloc(MOVE_SPACE, sizeof(*MCTS_termNodes), "Could not allocate memory for the MCTS terminal nodes.", true);
    MCTS_movArr = REC_calloc(MOVE_SPACE, sizeof(*MCTS_movArr), "Could not allocate memory for the MCTS move array.", true);

    Connect4 mctsC4;
    unsigned long long secs;
    Connect4_clone(_C4, &mctsC4);

    MCTSResult result = (MCTSResult){
        .root = &root,
        .wdl = &root.wdl,
        .trials = &MCTS_trials,
        .secs = &secs};

    thrd_t progThrd;
    REC_thrd_create(&progThrd, MCTSResult_thread, &result, "Could not create the MCTS progress thread.", true);

    for (MCTS_trials = secs = 0; atomic_load_explicit(&MCTS_run, memory_order_relaxed) && root.wdl == MCTS_NONE; MCTS_trials++)
    {
        MCTSNode *const restrict leaf = MCTSNode_Connect4_expand(MCTSNode_Connect4_select(&root, &mctsC4), &mctsC4);

        Connect4_play(&mctsC4, leaf->move);

        double reward;

        switch (leaf->wdl)
        {
        case MCTS_LOSS:
            reward = C4_variant == CONNECT4_MISERE ? -3.0 : 3.0;
            break;
        case MCTS_WIN:
            reward = C4_variant == CONNECT4_MISERE ? 3.0 : -3.0;
            break;
        case MCTS_DRAW:
            reward = 0.0;
            break;
        default:
            reward = MCTSNode_Connect4_simulate(&mctsC4);
            break;
        }

        MCTSNode_backpropagate(leaf, reward);
        Connect4_copy(_C4, &mctsC4);
    }

    secs = !secs ? 1 : secs;

    const MCTSNode *const restrict bestNode = MCTSNode_mostRobust(&root);

    result.move = bestNode->move;
    *result.wdl != MCTS_NONE ? MCTSResult_print(&result) : FTW_VOID_NOP;
    putchar('\n');

    atomic_store_explicit(&MCTS_run, false, memory_order_relaxed);
    REC_thrd_join(progThrd, nullptr, "Could not join the MCTS progress thread.", true);
    MCTSNode_destroy(&root);
    MCTSTable_destroy(&MCTS_table);
    MemoryPool_destroy(&MCTS_nodePool);
    MemoryPool_destroy(&MCTS_entryPool);
    Connect4_destroy(&mctsC4);
    REC_free(MCTS_termNodes);
    REC_free(MCTS_movArr);
    signal(SIGINT, SIG_DFL);

    return root.wdl;
}

#endif // MONTECARLO_H //
