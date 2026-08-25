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
    uint8_t move, count, /*nones, index; */ turn;
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

// static constexpr double MCTS_C = 1.4142135623730950488;

static MemoryPool MCTS_nodePool, MCTS_entryPool;
static unsigned long long MCTS_trials;
static MCTSTable MCTS_table;
static MCTSNode *restrict *restrict MCTS_termNodes;
static uint8_t *restrict MCTS_movArr, MCTS_termCnt;
static atomic_bool MCTS_run;

static bool (*MCTSNode_Connect4_evaluate)(MCTSNode *const restrict, const Connect4 *const restrict);
//static double (*MCTSNode_Connect4_simulate)(Connect4 *const restrict);
//static void (*MCTSResult_print)(const MCTSResult *const restrict);

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

///////////////////////////////////////////////////////////////
/// @brief  Evaluates a Connect 4 Pop 10 node for termination.
/// @param  _node
/// @param  _C4
///////////////////////////////////////////////////////////////
static inline bool MCTSNode_Connect4_pop10_evaluate(MCTSNode *const restrict _node, const Connect4_Pop10 *const restrict _P10)
{
    if (Connect4_pop10_over(_P10))
    {
        _node->wdl = MCTS_WIN;

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////
/// @brief  Evaluation function for a Make 7 MCTS node.
/// @param  _node
/// @param  _M7
////////////////////////////////////////////////////////
static inline bool MCTSNode_Make7_evaluate(MCTSNode *const restrict _node, const Make7 *const restrict _M7)
{
    if (Make7_targetSum(_M7))
    {
        _node->wdl = MCTS_LOSS;

        return true;
    }

    if (Make7_noMoreTiles(_M7))
    {
        _node->wdl = MCTS_DRAW;

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
/// @param  _node   Unaliased pointer to the node.
/// @param  _ancest Unaliased pointer to the ancestor.
/// @param  _INDEX  Index from the ancestor node.
/// @param  _MOVE   A move that led to this node.
///////////////////////////////////////////////////////////////
static inline void MCTSNode_init(MCTSNode *const restrict _node, MCTSNode *const restrict _ancest, const uint8_t _TURN /*_INDEX*/, const uint8_t _MOVE)
{
    _node->ancestor = _ancest;
    _node->turn = _TURN; // _node->index = _INDEX;
    _node->move = _MOVE;
    _node->descendants = nullptr;
    _node->visits = _node->score = 0.0;
    _node->count = /*_node->nones =*/ 0;
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
/*static inline void MCTSNode_swap(MCTSNode *const restrict _ancest, MCTSNode *const restrict _descend)
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
}*/

/////////////////////////////////////////////////////////////////
/// @brief  Proves a node's WDL outcome from its descendants.
/// @param  _node
/// @return `true` if proved; otherwise `false`.
/// @note   Derived from OR-node logic from Proof-Number Search.
/////////////////////////////////////////////////////////////////
static inline bool MCTSNode_prove(MCTSNode *const restrict _node)
{
    uint8_t losses = 0, nones = 0;
    bool draw = false;

    for (uint8_t i = 0; i < _node->count; i++)
    {
        const MCTSNode *const restrict child = _node->descendants[i];
        const bool SAME_TURN = child->turn == _node->turn;

        switch (child->wdl)
        {
        case MCTS_LOSS:
            if (!SAME_TURN)
            {
                _node->wdl = MCTS_WIN;
                MCTSNode_prune(_node);
                return true;
            }
            losses++;
            break;
        case MCTS_DRAW:
            draw = true;
            break;
        case MCTS_WIN:
            if (SAME_TURN)
            {
                _node->wdl = MCTS_WIN;
                MCTSNode_prune(_node);
                return true;
            }
            losses++;
            break;
        default:
            nones++;
            break;
        }
    }

    if (!nones)
    {
        if (draw)
        {
            _node->wdl = MCTS_DRAW;
            MCTSNode_prune(_node);
            return true;
        }
        else if (losses && losses == _node->count)
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
static inline MCTSNode *MCTSNode_Connect4_select(MCTSNode *restrict _node, Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    while (_node->descendants)
    {
        MCTSNode *restrict selected = nullptr;
        double currUCB1, bestUCB1 = -DBL_MAX;

        for (uint8_t i = 0; i < _node->count /*_node->nones*/; i++)
        {
            MCTSNode *const restrict candidate = _node->descendants[i];

            if (!candidate->visits)
            {
                return _node;
            }

            if (candidate->wdl == MCTS_NONE && (currUCB1 = MCTSNode_UCB1(candidate)) > bestUCB1)
            {
                bestUCB1 = currUCB1;
                selected = candidate;
            }
        }

        // TODO: Support PopOut/Pop 10 (swap + nones optimization)
        C4_variant == CONNECT4_POP10 ? Connect4_pop10_play(_c4, _p10, selected->move) : Connect4_play(_c4, selected->move);

        _node = selected;
    }

    return _node;
}

/////////////////////////////////////////////////////////////////////////////////
/// @brief  Selects a Make 7 node with the highest upper confidence bound score.
/// @param  _node
/// @param  _m7
/////////////////////////////////////////////////////////////////////////////////
static inline MCTSNode *MCTSNode_Make7_select(MCTSNode *restrict _node, Make7 *const restrict _m7)
{
    while (_node->descendants)
    {
        MCTSNode *restrict selected = nullptr;
        double currUCB1, bestUCB1 = -DBL_MAX;

        for (uint8_t i = 0; i < _node->count; i++)
        {
            MCTSNode *const restrict candidate = _node->descendants[i];

            if (!candidate->visits)
            {
                return _node;
            }

            if (candidate->wdl == MCTS_NONE && (currUCB1 = MCTSNode_UCB1(candidate)) > bestUCB1)
            {
                bestUCB1 = currUCB1;
                selected = candidate;
            }
        }

        Make7_drop(_m7, selected->move >> 3, selected->move & 7);

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
static inline MCTSNode *MCTSNode_Connect4_expand(MCTSNode *const restrict _node, Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    if (!_node->descendants)
    {
        const bool MCTS_POP10 = C4_variant == CONNECT4_POP10;

        MCTS_POP10 ? Connect4_pop10_generate(_c4, _p10, MCTS_movArr, &_node->count) : Connect4_generate(_c4, MCTS_movArr, &_node->count);

        _node->descendants = MemoryPool_alloc(&MCTS_nodePool, sizeof(*_node->descendants) * _node->count);
        MCTS_termCnt = 0;

        for (uint8_t i = 0; i < _node->count; i++)
        {
            //_node->nones++;

            MCTSNode *restrict *const restrict newNode = &_node->descendants[i];

            *newNode = MemoryPool_alloc(&MCTS_nodePool, sizeof(**newNode));
            MCTSNode_init(*newNode, _node, i, MCTS_movArr[i]);
            MCTS_POP10 ? Connect4_pop10_play(_c4, _p10, (*newNode)->move) : Connect4_play(_c4, (*newNode)->move);
            (*newNode)->key = Connect4_key(_c4) | (MCTS_POP10 * Connect4_pop10_key(_p10));
            (*newNode)->turn = MCTS_POP10 ? _p10->turn : !_node->turn;

            if (MCTS_POP10 ? MCTSNode_Connect4_pop10_evaluate(*newNode, _p10) : MCTSNode_Connect4_evaluate(*newNode, _c4))
            {
                MCTS_termNodes[MCTS_termCnt++] = *newNode;
                //MCTSNode_swap(_node, *newNode);
                MCTSTable_insert(&MCTS_table, (*newNode)->key, (*newNode)->wdl);
            }

            MCTS_POP10 ? Connect4_pop10_unplay(_c4, _p10) : Connect4_unplay(_c4);
        }

        MCTSNode *const restrict tNode = MCTS_termCnt ? MCTS_termNodes[Xoshiro128pp_nextN(&g_rng, MCTS_termCnt)] : nullptr;

        if (tNode && tNode->wdl == (MCTS_POP10 ? MCTS_WIN : MCTS_LOSS))
        {
            return tNode;
        }
    }

    return _node->descendants[Xoshiro128pp_nextN(&g_rng, _node->count)];
}

/////////////////////////////////////////////////////////////////////////
/// @brief  Expands all moves of a Make 7 node and selects a random one.
/// @param  _node
/// @param  _m7
/////////////////////////////////////////////////////////////////////////
static inline MCTSNode *MCTSNode_Make7_expand(MCTSNode *const restrict _node, const Make7 *const restrict _M7)
{
    if (!_node->descendants)
    {
        Make7_generate(_M7, MCTS_movArr, &_node->count);

        _node->descendants = MemoryPool_alloc(&MCTS_nodePool, sizeof(*_node->descendants) * _node->count);
        MCTS_termCnt = 0;

        for (uint8_t i = 0; i < _node->count; i++)
        {
            MCTSNode *restrict *const restrict newNode = &_node->descendants[i];

            *newNode = MemoryPool_alloc(&MCTS_nodePool, sizeof(**newNode));
            MCTSNode_init(*newNode, _node, !_node->turn, MCTS_movArr[i]);

            Make7 expM7 = *_M7;

            Make7_drop(&expM7, (*newNode)->move >> 3, (*newNode)->move & 7);
            (*newNode)->key = Make7_lock(&expM7);

            if (MCTSNode_Make7_evaluate(*newNode, &expM7))
            {
                MCTS_termNodes[MCTS_termCnt++] = *newNode;
                MCTSTable_insert(&MCTS_table, (*newNode)->key, (*newNode)->wdl);
            }
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
/// @param  _p10
/// @return A reward for the player who initiated it.
/////////////////////////////////////////////////////////
static inline double MCTSNode_Connect4_simulate(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    const bool SIM_POP10 = C4_variant == CONNECT4_POP10;
    const bool SIM_TURN = SIM_POP10 ? _p10->turn : _c4->plies & 1;

    uint8_t simCnt; MCTSWDL simWDL;

    for (;;)
    {
        if ((simWDL = MCTSTable_WDL(&MCTS_table, Connect4_key(_c4) | (SIM_POP10 * Connect4_pop10_key(_p10)))) != MCTS_NONE)
        {
            const double SCORE = simWDL == MCTS_WIN ? 2.0 : simWDL == MCTS_LOSS ? -2.0 : 0.0;

            return SIM_TURN == (SIM_POP10 ? _p10->turn : (_c4->plies & 1)) ? -SCORE : SCORE;
        }

        SIM_POP10 ? Connect4_pop10_generate(_c4, _p10, MCTS_movArr, &simCnt) : Connect4_generate(_c4, MCTS_movArr, &simCnt);

        if (simCnt && _c4->plies < UINT16_MAX)
        {
            const uint8_t SIM_MOVE = MCTS_movArr[Xoshiro128pp_nextN(&g_rng, simCnt)];

            SIM_POP10 ? Connect4_pop10_play(_c4, _p10, SIM_MOVE) : Connect4_play(_c4, SIM_MOVE);

            if (SIM_POP10 ? Connect4_pop10_over(_p10) : Connect4_fourInARow(_c4->side ^ _c4->mask))
            {
                switch (C4_variant)
                {
                default:
                    return SIM_TURN == (_c4->plies & 1) ? 1.0 : -1.0;
                case CONNECT4_MISERE:
                    return SIM_TURN == (_c4->plies & 1) ? -1.0 : 1.0;
                case CONNECT4_POP10:
                    return SIM_TURN == _p10->turn ? 1.0 : -1.0;
                }
            }
        }
        else
        {
            return 0.0;
        }
    }
}

///////////////////////////////////////////////////////////////////////////
/// @brief  Simulates a Make 7 game and rewards the player who started it.
/// @param  _m7
///////////////////////////////////////////////////////////////////////////
static inline double MCTSNode_Make7_simulate(Make7 *const restrict _m7)
{
    const bool SIM_TURN = Make7_moves(_m7) & 1;

    uint8_t simCnt; MCTSWDL simWDL; bool playTurn = SIM_TURN;

    for (;;)
    {
        if ((simWDL = MCTSTable_WDL(&MCTS_table, Make7_lock(_m7))) != MCTS_NONE)
        {
            const double SCORE = simWDL == MCTS_WIN ? 2.0 : simWDL == MCTS_LOSS ? -2.0 : 0.0;

            return SIM_TURN == playTurn ? -SCORE : SCORE;
        }

        Make7_generate(_m7, MCTS_movArr, &simCnt);

        if (simCnt)
        {
            const uint8_t SIM_MOVE = MCTS_movArr[Xoshiro128pp_nextN(&g_rng, simCnt)];

            Make7_drop(_m7, SIM_MOVE >> 3, SIM_MOVE & 7);

            playTurn = !playTurn;

            if (Make7_targetSum(_m7))
            {
                return SIM_TURN == playTurn ? 1.0 : -1.0;
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
/// @param  _node   Leaf node.
/// @param  _score  Playout score.
///////////////////////////////////////////////////////////////////
static inline void MCTSNode_backpropagate(MCTSNode *restrict _node, double _score)
{
    while (_node)
    {
        _node->visits++;
        _node->score += _score;

        MCTSNode *const restrict parent = _node->ancestor;

        if (MCTSNode_prove(_node) /*&& parent*/)
        {
            MCTSTable_insert(&MCTS_table, _node->key, _node->wdl);
            //MCTSNode_swap(parent, _node);
        }

        if (parent && parent->turn != _node->turn)
        {
            _score = -_score;
        }

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
static inline void MCTSResult_print(const MCTSResult *const restrict _RES)
{
    const unsigned long long M_SPEED = *_RES->trials / *_RES->secs;
    const bool M_MAKE7 = C4_variant == CONNECT4_MAKE7;
    const uint8_t M_MOVE = _RES->move; char mChar;

    if (!M_MAKE7)
    {
        mChar = M_MOVE + (COLS < 10 ? '1' : 'A');

        if (M_MOVE >= COLS)
        {
            mChar = M_MOVE + (COLS < 10 ? 'A' : 'a') - COLS;
        }
    }

    const char M_NOTE_A = M_MAKE7 ? (_RES->move >> 3) + '1' : mChar;
    const char M_NOTE_B = M_MAKE7 * ((_RES->move & 7) + 'A');

    switch (*_RES->wdl)
    {
    case MCTS_WIN:
        printf("\r\e[1;92m%c%c %s\e[0m %llu %llu %llu        ", M_NOTE_A, M_NOTE_B, FTW_STR_WIN, *_RES->trials, M_SPEED, *_RES->secs);
        break;
    case MCTS_DRAW:
        printf("\r\e[1;93m%c%c %s\e[0m %llu %llu %llu        ", M_NOTE_A, M_NOTE_B, FTW_STR_DRAW, *_RES->trials, M_SPEED, *_RES->secs);
        break;
    case MCTS_LOSS:
        printf("\r\e[1;91m%c%c %s\e[0m %llu %llu %llu        ", M_NOTE_A, M_NOTE_B, FTW_STR_LOSS, *_RES->trials, M_SPEED, *_RES->secs);
        break;
    default:
        printf("\r\e[1m%c%c\e[0m %.3f %llu %llu %llu ", M_NOTE_A, M_NOTE_B, _RES->reward, *_RES->trials, M_SPEED, *_RES->secs);
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
    constexpr struct timespec _1_SEC = { .tv_sec = 1, .tv_nsec = 0 };

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
    //MCTSNode_Connect4_simulate = MCTSNode_Connect4_original_simulate;
    //MCTSResult_print = MCTSResult_Connect4_print;

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

////////////////////////////////////////////////////////////////////
/// @brief  Begins a Monte Carlo Tree Search on Connect 4 / Make 7.
/// @param  _C4
/// @param  _P10
/// @param  _M7
/// @return Game outcome (`WIN`, `DRAW`, `LOSS`, or `NONE`).
////////////////////////////////////////////////////////////////////
static inline MCTSWDL MonteCarloTreeSearch(const Connect4 *const restrict _C4, const Connect4_Pop10 *const restrict _P10, const Make7 *const restrict _M7)
{
    const bool MCTS_POP10 = C4_variant == CONNECT4_POP10;
    const bool MCTS_MAKE7 = C4_variant == CONNECT4_MAKE7;

    MCTSNode rootNode;

    {
        uint8_t rootTurn;

        if (MCTS_POP10)
        {
            rootTurn = _P10->turn;
        }
        else if (MCTS_MAKE7)
        {
            rootTurn = Make7_moves(_M7) & 1;
        }
        else
        {
            rootTurn = _C4->plies & 1;
        }

        MCTSNode_init(&rootNode, nullptr, rootTurn, 0);
    }

    {
        Board rootKey = Connect4_key(_C4);

        if (MCTS_POP10)
        {
            rootKey |= Connect4_pop10_key(_P10);
        }
        else if (MCTS_MAKE7)
        {
            rootKey = Make7_lock(_M7);
        }

        rootNode.key = rootKey;
    }

    MCTSTable_init(&MCTS_table, NS_table.size);
    MemoryPool_init(&MCTS_nodePool);
    MemoryPool_init(&MCTS_entryPool);
    atomic_init(&MCTS_run, true);
    signal(SIGINT, MCTS_stopSearch);

    MCTS_termNodes = REC_calloc(MOVE_SPACE, sizeof(*MCTS_termNodes), "Could not allocate memory for the MCTS terminal nodes.", true);
    MCTS_movArr = REC_calloc(MOVE_SPACE, sizeof(*MCTS_movArr), "Could not allocate memory for the MCTS move array.", true);

    Connect4 mctsC4;
    Connect4_Pop10 mctsP10 = *_P10;

    Connect4_clone(_C4, &mctsC4);

    Make7 mctsM7 = *_M7;

    unsigned long long secs;

    MCTSResult result = (MCTSResult)
    {
        .root = &rootNode,
        .wdl = &rootNode.wdl,
        .trials = &MCTS_trials,
        .secs = &secs
    };

    thrd_t progThrd;

    REC_thrd_create(&progThrd, MCTSResult_thread, &result, "Could not create the MCTS progress thread.", true);

    for (MCTS_trials = secs = 0; atomic_load_explicit(&MCTS_run, memory_order_relaxed) && rootNode.wdl == MCTS_NONE; MCTS_trials++)
    {
        MCTSNode *restrict leaf;

        if (MCTS_MAKE7)
        {
            leaf = MCTSNode_Make7_expand(MCTSNode_Make7_select(&rootNode, &mctsM7), &mctsM7);
            Make7_drop(&mctsM7, leaf->move >> 3, leaf->move & 7);
        }
        else
        {
            leaf = MCTSNode_Connect4_expand(MCTSNode_Connect4_select(&rootNode, &mctsC4, &mctsP10), &mctsC4, &mctsP10);
            MCTS_POP10 ? Connect4_pop10_play(&mctsC4, &mctsP10, leaf->move) : Connect4_play(&mctsC4, leaf->move);
        }

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
            reward = MCTS_MAKE7 ? MCTSNode_Make7_simulate(&mctsM7) : MCTSNode_Connect4_simulate(&mctsC4, &mctsP10);
            break;
        }

        MCTSNode_backpropagate(leaf, reward);

        if (MCTS_MAKE7)
        {
            mctsM7 = *_M7;
        }
        else
        {
            Connect4_copy(_C4, &mctsC4);
            mctsP10 = *_P10;
        }
    }

    secs = !secs ? 1 : secs;

    const MCTSNode *const restrict bestNode = MCTSNode_mostRobust(&rootNode);

    result.move = bestNode->move;
    *result.wdl != MCTS_NONE ? MCTSResult_print(&result) : FTW_VOID_NOP;
    putchar('\n');

    atomic_store_explicit(&MCTS_run, false, memory_order_relaxed);
    REC_thrd_join(progThrd, nullptr, "Could not join the MCTS progress thread.", true);
    MCTSNode_destroy(&rootNode);
    MCTSTable_destroy(&MCTS_table);
    MemoryPool_destroy(&MCTS_nodePool);
    MemoryPool_destroy(&MCTS_entryPool);
    Connect4_destroy(&mctsC4);
    REC_free(MCTS_termNodes);
    REC_free(MCTS_movArr);
    signal(SIGINT, SIG_DFL);

    return rootNode.wdl;
}

#endif // MONTECARLO_H //
