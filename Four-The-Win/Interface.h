/*
 *  Author: 2026- TheTrustedComputer
 *
 *  This is the command-line interface, a simple REPL that allows users to interact with the solver.
 *  The actual WebAssembly APIs and related helpers are located in the file WebAsmAPI.h.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

static Connect4 UI_c4;
static Connect4_Pop10 UI_p10;
static Make7 UI_m7, UI_m7UndoBuff[MAKE7_AREA];
static PerftStat UI_perft;

static bool UI_run = true;
static bool UI_c4NotReady = true;
static bool UI_ttNotReady = true;

static uint8_t UI_cols = 7;
static uint8_t UI_rows = 6;
static uint8_t UI_ttMulti = 4;
static uint8_t UI_m7UndoIdx;
static uint8_t UI_m7MoveHist[MAKE7_AREA];

static uint32_t UI_ttSize = TT_BASE_SIZE;

//////////////////////////////////////////////
/// @brief  A help menu for invalid commands.
//////////////////////////////////////////////
static inline void Interface_help(void)
{
    puts(FTW_STR_CMD_DESC);
    puts(" [1-0]      Drop a disk into column [1-10] (Connect 4).");
    puts(" [A-J]      Pop a disk from column [1-10] (PopOut/Pop 10).");
    puts(" [1-3][A-G] Place a tile [1-3] into column [1-7] (Make 7).");
    puts(" <          Shift one column to the left (Cylinder).");
    puts(" >          Shift one column to the right (Cylinder).");
#ifdef FTW_SQLITE
    puts(" delete     Delete all results from the database.");
#endif
    puts(" emoji      Enable board emojis (requires font support).");
    printf(" exit       %s Alternative: quit.\n", FTW_STR_CLOSE_APP);
    printf(" help       Display available commands. %s: ?.\n", FTW_STR_SHORT_ALIAS);
    printf(" hist       Show the move history. %s: y.\n", FTW_STR_SHORT_ALIAS);
#ifdef FTW_SQLITE
    puts(" insert R   Insert a result R into the database manually.");
#endif
    printf(" list       List possible moves. %s: z.\n", FTW_STR_SHORT_ALIAS);
    printf(" mcts       Run a Monte Carlo tree search. %s: t.\n", FTW_STR_SHORT_ALIAS);
    printf(" moves      Count the number of moves made. %s: x.\n", FTW_STR_SHORT_ALIAS);
#ifdef FTW_SQLITE
    puts(" oracle D   Generate a move oracle to depth D (DFS/BFS).");
#endif
    puts(" perft D    Run a move-generation performance test to depth D.");
    printf(" play       Calculate and play the best move. %s: w.\n", FTW_STR_SHORT_ALIAS);
    puts(" pops       Output collected disks per player (Pop 10).");
    printf(" print      Pretty print the game board. %s: -.\n", FTW_STR_SHORT_ALIAS);
    printf(" quit       %s %s: q.\n", FTW_STR_CLOSE_APP, FTW_STR_SHORT_ALIAS);
    printf(" restart    Restart to the initial state. %s: r.\n", FTW_STR_SHORT_ALIAS);
    puts(" ruleset R  Switch the ruleset to R, or get the active one.");
    puts(" size W H   Set the game board dimensions to W x H (Connect 4).");
    printf(" solve      Solve this position. %s: s.\n", FTW_STR_SHORT_ALIAS);
    puts(" table T    Adjust the transposition table size (T = [inc/dec]).");
    printf(" tgwin      Toggle the Make 7 win method (%s/%s).\n", FTW_STR_EXACT, FTW_STR_WINDOWING);
    puts(" tiles      Obtain the remaining tiles per player (Make 7).");
    printf(" undo       Undo the last move. %s: u.\n", FTW_STR_SHORT_ALIAS);
    printf(" view       View solutions for each move. %s: v.\n", FTW_STR_SHORT_ALIAS);
}

//////////////////////////////////////////////////////////
/// @brief          Finds a long or exact command.
/// @param  _CMD    Command string.
/// @param  _TGT    Target string.
/// @return         `true` if matched; otherwise `false`.
//////////////////////////////////////////////////////////
static inline bool Interface_findLongCmd(const char *const restrict _CMD, const char *const restrict _TGT)
{
    return !strcmp(_CMD, _TGT);
}

//////////////////////////////////////////////////////////
/// @brief          Finds a short or long command.
/// @param  _CMD    Command string.
/// @param  _S_TGT  Short target.
/// @param  _L_TGT  Long target.
/// @return         `true` if matched; otherwise `false`.
//////////////////////////////////////////////////////////
static inline bool Interface_findShortCmd(const char *const restrict _CMD, const char _S_TGT, const char *const restrict _L_TGT)
{
    return (*_CMD == _S_TGT && !_CMD[1]) || Interface_findLongCmd(_CMD, _L_TGT);
}

///////////////////////////////////////////////////
/// @brief  Lists legal moves in a Connect 4 game.
/// @param  _C4
/// @param  _CP
/// @param  _POP
///////////////////////////////////////////////////
static inline void Interface_Connect4_list(const Connect4 *const restrict _C4, const Connect4_Pop10 *const restrict _CP, const bool _POPOUT, const bool _POP10)
{
    uint8_t i;

    if (_POP10)
    {
        if (Connect4_pop10_passForced(_C4, _CP))
        {
            putchar(POP10_PASS_CHR);
        }
        else
        {
            if (_CP->phase)
            {
                for (i = 0; i < COLS; i++)
                {
                    if (Connect4_poppable(_C4, i))
                    {
                        printf("%c ", i + 'A');
                    }
                }
            }
            else
            {
                const uint8_t NON_FULLS = Connect4_nonFullCols(_C4);
                const uint8_t HIST_COL = _C4->hist[_C4->plies - 1] - COLS;

                for (i = 0; i < COLS; i++)
                {
                    if ((NON_FULLS == 1 || (NON_FULLS >= 2 && i != HIST_COL)) && Connect4_droppable(_C4, i))
                    {
                        printf("%c ", i + '1');
                    }
                }
            }
        }
    }
    else
    {
        for (i = 0; i < COLS; i++)
        {
            if (Connect4_droppable(_C4, i))
            {
                printf("%c ", i + '1');
            }
        }

        for (i = 0; _POPOUT && i < COLS; i++)
        {
            if (Connect4_poppable(_C4, i))
            {
                printf("%c ", i + 'A');
            }
        }
    }

    putchar('\n');
}

static inline void Interface_Make7_list(const Make7 *const restrict _M7)
{
    bool hasMoves = false;

    for (uint8_t i = 0; i < 3; i++)
    {
        for (uint8_t j = 0; j < MAKE7_SIZE; j++)
        {
            if (Make7_droppable(_M7, i, j))
            {
                printf("%c%c ", i + '1', j + 'A');
                hasMoves = true;
            }
        }
    }

    hasMoves ? putchar('\n') : FTW_VOID_NOP;
}

//////////////////////////////////////////////////////////////////
/// @brief  Utility function to print the moves of a Make 7 game.
/// @param  _M7
//////////////////////////////////////////////////////////////////
static inline void Interface_Make7_moves(const Make7 *const restrict _M7)
{
    const uint8_t M7_MOVES = Make7_moves(_M7);

    for (uint8_t i = 0; i < M7_MOVES; i++)
    {
        printf("%c%c", (UI_m7MoveHist[i] >> 3) + '1', (UI_m7MoveHist[i] & 7) + 'A');
    }

    M7_MOVES ? putchar('\n') : FTW_VOID_NOP;
}

////////////////////////////////////////////////////////
/// @brief  Returns the Connect 4 Pop 10 secondary key.
/// @param  _P10
////////////////////////////////////////////////////////
static inline Board Interface_Connect4_pop10_2ndKey(const Connect4_Pop10 *const restrict _P10)
{
    return C4_variant == CONNECT4_POP10 ? Connect4_pop10_stateKey(_P10) : UINT64_MAX;
}

///////////////////////////////////////////////////////
/// @brief  Obtains the player's turn or side to move.
/// @param  _C4_PLIES
/// @param  _P_TURN
///////////////////////////////////////////////////////
static inline bool Interface_Connect4_turn(const uint16_t _C4_PLIES, const bool _P_TURN)
{
    return C4_variant == CONNECT4_POP10 ? _P_TURN : _C4_PLIES & 1;
}

////////////////////////////////////////////////////////////////////
/// @brief  Helper routine to stamp the winner of a Connect 4 game.
/// @param  _C4
/// @param  _P10
/// @return The value of `Connect4_*_winner()`.
////////////////////////////////////////////////////////////////////
static inline int Interface_Connect4_winner(const Connect4 *const restrict _C4, const Connect4_Pop10 *const restrict _P10)
{
    return C4_variant == CONNECT4_POP10 ? Connect4_pop10_winner(_P10) : Connect4_winner(_C4);
}

#ifdef FTW_SQLITE
////////////////////////////////////////////////////////////////////
/// @brief  Depth-first searcher to create a Connect 4 move oracle.
/// @param  _c4
/// @param  _DEP
/// @param  _POP
////////////////////////////////////////////////////////////////////
static inline void Interface_Connect4_oracle_dfs(Connect4 *const restrict _c4, const uint16_t _DEP, const bool _POP)
{
    if (_DEP)
    {
        const Board C4_KEY = Connect4_key(_c4); Result dfsR;

        if (!SQLite_Connect4_query(database, C4_KEY, &dfsR))
        {
            TransTable_freeClear(&NS_table);
            dfsR = NegaScout_Connect4_iterative(_c4, false);
            SQLite_Connect4_insert(database, C4_KEY, &dfsR);
            putchar('\r');
            _c4->plies ? Connect4_moves(_c4), putchar('\n') : printf("\u2205");
            fflush(stdout);
        }
        else
        {
            return;
        }

        if (_POP)
        {
            uint8_t dDrop[MOVE_BOUNDS], dDropCnt = 0;
            uint8_t dPop[MOVE_BOUNDS], dPopCnt = 0, i;

            Connect4_original_genMoveBody((_c4->mask + BOT_MASK) & ALL_MASK, dDrop, &dDropCnt);
            Connect4_original_genMoveBody(_c4->side & BOT_MASK, dPop, &dPopCnt);

            const bool C4_SYMM = Connect4_symmetric(_c4);

            C4_SYMM ? (dDropCnt = (dDropCnt >> 1) + (dDropCnt & 1)) : FTW_VOID_NOP;
            C4_SYMM ? (dPopCnt = (dPopCnt >> 1) + (dPopCnt & 1)) : FTW_VOID_NOP;

            for (i = 0; i < dDropCnt; i++)
            {
                Connect4_drop(_c4, dDrop[i]);
                !Connect4_over(_c4) ? Interface_Connect4_oracle_dfs(_c4, _DEP - 1, true) : FTW_VOID_NOP;
                Connect4_undrop(_c4);
            }

            for (i = 0; i < dPopCnt; i++)
            {
                Connect4_pop(_c4, dPop[i]);
                !Connect4_over(_c4) ? Interface_Connect4_oracle_dfs(_c4, _DEP - 1, true) : FTW_VOID_NOP;
                Connect4_unpop(_c4);
            }
        }
        else
        {
            uint8_t dArr[MOVE_BOUNDS], dCnt = 0;

            Connect4_original_genMoveBody((_c4->mask + BOT_MASK) & ALL_MASK, dArr, &dCnt);
            Connect4_symmetric(_c4) ? (dCnt = (dCnt >> 1) + (dCnt & 1)) : FTW_VOID_NOP;

            for (uint8_t i = 0; i < dCnt; i++)
            {
                Connect4_play(_c4, dArr[i]);
                !Connect4_over(_c4) ? Interface_Connect4_oracle_dfs(_c4, _DEP - 1, false) : FTW_VOID_NOP;
                Connect4_unplay(_c4);
            }
        }
    }
}

///////////////////////////////////////////////////////////////
/// @brief      Move oracle database builder for Connect 4.
/// @param _c4  Unaliased pointer to the Connect 4 game state.
/// @param _DEP The maximum search depth.
/// @param _DFS `true` = depth-first; `false` = breadth-first.
/// @param _POP A flag to indicate the ruleset is PopOut.
///////////////////////////////////////////////////////////////
static inline void Interface_Connect4_oracle(Connect4 *const restrict _c4, const uint16_t _DEP, const bool _DFS, const bool _POP)
{
    SQLite_beginTransaction(database);

    if (_DFS) // O(b^d) time; O(b*d) space
    {
        Interface_Connect4_oracle_dfs(_c4, _DEP, _POP);
    }
    else // O(b^d) time and space
    {
        QueueList queue;

        QueueList_init(&queue);
        QueueList_push(&queue, _c4);

        for (uint16_t plies = _c4->plies, level = 0; !QueueList_empty(&queue);)
        {
            Connect4 *const restrict bC4 = queue.head->data;

            if (plies != bC4->plies)
            {
                plies = bC4->plies;
                level++;
            }

            if (level == _DEP)
            {
                break;
            }

            QueueList_pop(&queue);

            const Board C4_KEY = Connect4_key(bC4); Result bfsR;

            if (!SQLite_Connect4_query(database, C4_KEY, &bfsR))
            {
                TransTable_freeClear(&NS_table);
                bfsR = NegaScout_Connect4_iterative(bC4, false);
                SQLite_Connect4_insert(database, C4_KEY, &bfsR);
                putchar('\r');
                bC4->plies ? Connect4_moves(bC4), putchar('\n') : printf("\u2205");
                fflush(stdout);
            }
            else
            {
                if (bC4 != _c4)
                {
                    Connect4_destroy(bC4);
                    REC_free(bC4);
                }

                continue;
            }

            Connect4 *restrict nC4;

            if (_POP)
            {
                uint8_t bDrop[MOVE_BOUNDS], bDropCnt = 0;
                uint8_t bPop[MOVE_BOUNDS], bPopCnt = 0, i;

                Connect4_original_genMoveBody((bC4->mask + BOT_MASK) & ALL_MASK, bDrop, &bDropCnt);
                Connect4_original_genMoveBody(bC4->side & BOT_MASK, bPop, &bPopCnt);

                const bool C4_SYMM = Connect4_symmetric(bC4);

                C4_SYMM ? (bDropCnt = (bDropCnt >> 1) + (bDropCnt & 1)) : FTW_VOID_NOP;
                C4_SYMM ? (bPopCnt = (bPopCnt >> 1) + (bPopCnt & 1)) : FTW_VOID_NOP;

                for (i = 0; i < bDropCnt; i++)
                {
                    Connect4_drop(bC4, bDrop[i]);

                    if (!Connect4_over(bC4))
                    {
                        nC4 = REC_calloc(1, sizeof(*nC4), "Could not allocate memory for the next PopOut game state (drops).", true);

                        Connect4_clone(bC4, nC4);
                        QueueList_push(&queue, nC4);
                    }

                    Connect4_undrop(bC4);
                }

                for (i = 0; i < bPopCnt; i++)
                {
                    Connect4_pop(bC4, bPop[i]);

                    if (!Connect4_over(bC4))
                    {
                        nC4 = REC_calloc(1, sizeof(*nC4), "Could not allocate memory for the next PopOut game state (pops).", true);

                        Connect4_clone(bC4, nC4);
                        QueueList_push(&queue, nC4);
                    }

                    Connect4_unpop(bC4);
                }
            }
            else
            {
                uint8_t bArr[MOVE_BOUNDS], bCnt = 0;

                Connect4_original_genMoveBody((bC4->mask + BOT_MASK) & ALL_MASK, bArr, &bCnt);
                Connect4_symmetric(bC4) ? (bCnt = (bCnt >> 1) + (bCnt & 1)) : FTW_VOID_NOP;

                for (uint8_t i = 0; i < bCnt; i++)
                {
                    Connect4_play(bC4, bArr[i]);

                    if (!Connect4_over(bC4))
                    {
                        nC4 = REC_calloc(1, sizeof(*nC4), "Could not allocate memory for the next Connect 4 game state.", true);

                        Connect4_clone(bC4, nC4);
                        QueueList_push(&queue, nC4);
                    }

                    Connect4_unplay(bC4);
                }
            }

            if (bC4 != _c4)
            {
                Connect4_destroy(bC4);
                REC_free(bC4);
            }
        }

        for (QueueNode *qNode = queue.head; qNode; qNode = qNode->next)
        {
            Connect4 *const restrict qC4 = qNode->data;

            if (qC4 != _c4)
            {
                Connect4_destroy(qC4);
                REC_free(qC4);
            }
        }

        QueueList_destroy(&queue);
    }

    SQLite_commitTransaction(database);
}
#endif

////////////////////////////////////////////////////////////
/// @brief  Move generation and speed tester for Connect 4.
/// @param  _stats
/// @param  _c4
/// @param  _D
////////////////////////////////////////////////////////////
void Interface_Connect4_perft(PerftStat *const restrict _stats, Connect4 *const restrict _c4, const uint16_t _D)
{
    uint8_t moveArr[MOVE_BOUNDS], moveCnt, i;

    Connect4_generateAll(_c4, moveArr, &moveCnt);

    if (_D == 1)
    {
        _stats->nodes += moveCnt;

        for (i = 0; i < moveCnt; i++)
        {
            moveArr[i] < COLS ? _stats->drops++ : _stats->pops++;
        }

        return;
    }

    for (i = 0; i < moveCnt; i++)
    {
        const uint8_t MOVE_IDX = moveArr[i];

        MOVE_IDX < COLS ? _stats->drops++ : _stats->pops++;

        Connect4_play(_c4, MOVE_IDX);
        !Connect4_over(_c4) ? Interface_Connect4_perft(_stats, _c4, _D - 1) : _stats->nodes++;
        Connect4_unplay(_c4);
    }
}

///////////////////////////////////////
/// @brief  Ditto, adapted for Pop 10.
/// @param  _stats
/// @param  _c4
/// @param  _p10
/// @param  _D
///////////////////////////////////////
void Interface_Connect4_pop10_perft(PerftStat *const restrict _stats, Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const uint16_t _D)
{
    uint8_t moveArr[MOVE_BOUNDS], moveCnt, moveIdx, i;

    Connect4_pop10_generate(_c4, _p10, moveArr, &moveCnt);

    if (_D == 1)
    {
        _stats->nodes += moveCnt;

        for (i = 0; i < moveCnt; i++)
        {
            (moveIdx = moveArr[i]) < COLS ? _stats->drops++ : (moveIdx == POP10_PASS ? _stats->passes++ : _stats->pops++);
        }

        return;
    }

    for (i = 0; i < moveCnt; i++)
    {
        (moveIdx = moveArr[i]) < COLS ? _stats->drops++ : (moveIdx == POP10_PASS ? _stats->passes++ : _stats->pops++);

        Connect4_pop10_play(_c4, _p10, moveIdx);
        !Connect4_pop10_over(_p10) ? Interface_Connect4_pop10_perft(_stats, _c4, _p10, _D - 1) : _stats->nodes++;
        Connect4_pop10_unplay(_c4, _p10);
    }
}

///////////////////////////////////////////
/// @brief  The Make 7 performance tester.
/// @param  _stats
/// @param  _m7
/// @param  _D
///////////////////////////////////////////
void Interface_Make7_perft(PerftStat *const restrict _stats, const Make7 *const restrict _M7, const uint8_t _D)
{
    uint8_t moveArr[MAKE7_SIZE_X3], moveCnt, moveIdx, i;

    Make7_generate(_M7, moveArr, &moveCnt);

    if (_D == 1)
    {
        _stats->nodes += moveCnt;

        for (i = 0; i < moveCnt; i++)
        {
            moveIdx = moveArr[i];

            switch (moveIdx >> 3)
            {
            case 0:
                _stats->tile1s++;
                break;
            case 1:
                _stats->tile2s++;
                break;
            case 2:
                _stats->tile3s++;
            default:
                break;
            }
        }

        return;
    }

    Make7 perftM7 = *_M7;

    for (i = 0; i < moveCnt; i++)
    {
        moveIdx = moveArr[i];

        const uint8_t MOVE_TILE = moveIdx >> 3;

        switch (MOVE_TILE)
        {
        case 0:
            _stats->tile1s++;
            break;
        case 1:
            _stats->tile2s++;
            break;
        case 2:
            _stats->tile3s++;
        default:
            break;
        }

        Make7_drop(&perftM7, MOVE_TILE, moveIdx & 7);
        !Make7_over(&perftM7) ? Interface_Make7_perft(_stats, &perftM7, _D - 1) : _stats->nodes++;
        perftM7 = *_M7;
    }
}

////////////////////////////////////////////////////////////////
/// @brief          Universal game performance tester.
/// @param  _stats  Structure to record performance statistics.
/// @param  _c4     Current Connect 4 game state.
/// @param  _p10    '' Pop 10 '' ''.
/// @param  _m7     '' Make 7 '' ''.
/// @param  _D      Maximum depth to evaluate.
////////////////////////////////////////////////////////////////
void Interface_perft(PerftStat *const restrict _stats, Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, Make7 *const restrict _m7, const uint8_t _D)
{
    struct timespec perftBegin, perftEnd;

    memset(_stats, 0, sizeof(*_stats));
    FourTheWin_monoTime(&perftBegin);

    if (_D)
    {
        switch (C4_variant)
        {
        default:
            Interface_Connect4_perft(_stats, _c4, _D);
            break;
        case CONNECT4_POP10:
            Interface_Connect4_pop10_perft(_stats, _c4, _p10, _D);
            break;
        case CONNECT4_MAKE7:
            Interface_Make7_perft(_stats, _m7, _D);
            break;
        }
    }
    else
    {
        _stats->nodes = 1;
    }

    FourTheWin_monoTime(&perftEnd);

    _stats->time = (double)(perftEnd.tv_sec - perftBegin.tv_sec) + (double)(perftEnd.tv_nsec - perftBegin.tv_nsec) / 1e9;

    C4_variant == CONNECT4_MAKE7 ? PerftStat_Make7_print(_stats) : PerftStat_Connect4_print(_stats);
}

//////////////////////////////////////
/// @brief  Main user interface loop.
//////////////////////////////////////
static inline void Interface_run(void)
{
    const bool UI_IS_CYLINDER = C4_variant == CONNECT4_CYLINDER;
    const bool UI_IS_POPOUT = C4_variant == CONNECT4_POPOUT;
    const bool UI_IS_POP10 = C4_variant == CONNECT4_POP10;
    const bool UI_IS_POPOUT_OR_POP10 = UI_IS_POPOUT || UI_IS_POP10;
    const bool UI_IS_MAKE7 = C4_variant == CONNECT4_MAKE7;

    TransTable_init(&NS_table, UI_ttSize * UI_ttMulti);
    HashRing_reset();

    UI_ttNotReady ? printf(FTW_STR_TRANSTABLE, NS_table.size), UI_ttNotReady = false : FTW_VOID_NOP;

    if (UI_IS_MAKE7)
    {
        M7_targetMethod = true;
        UI_c4NotReady ? (puts(FTW_STR_MAKE7_RULESET), UI_c4NotReady = false) : FTW_VOID_NOP;
        Make7_setTargetMethod();
        printf("Using the %s win method\n", M7_targetMethod ? FTW_STR_WINDOWING : FTW_STR_EXACT);
        Make7_init(&UI_m7);
        Make7_print(&UI_m7);
    }
    else
    {
        if (UI_c4NotReady)
        {
            printf(FTW_STR_PLAYING_RULSET, FTW_STR_CONNECT4, UI_IS_POP10 ? FTW_STR_POP10 : FTW_STR_VARIANTS[C4_variant]);
            UI_c4NotReady = false;
        }

        Connect4_prepare(UI_cols, UI_rows);
        Connect4_init(&UI_c4);
        UI_IS_POP10 ? Connect4_pop10_reset(&UI_c4, &UI_p10) : FTW_VOID_NOP;
        Connect4_funcPtrs_init();
        Connect4_globals_init();
        Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
        UI_IS_POP10 ? Connect4_pop10_print(&UI_p10) : FTW_VOID_NOP;

        HashRing_push(Connect4_canonicalize(Connect4_key(&UI_c4)));
        PathTable_init(&NS_path, UINT16_MAX);
        PathStack_init(&NS_stack);
        PathGraph_init(&NS_graph);
    }

    NegaScout_History_init();
    NegaScout_funcPtrs_init();
    MCTS_funtPtrs_init();
    Xoshiro128_init(&g_rng);

    char *restrict cmd, *arg, popTokens[COLS_X2 + 2], bestMove = '\0', policyMove, m7MovChr[2];
    Board c4Key, ptKey; uint64_t m7Key, t1Key, t2Key; Result result; size_t i, cmdLen; long cmdCnt; int winner;

#ifdef FTW_SQLITE
    char dbName[32]; bool dbQuery;

    UI_IS_MAKE7 ? sprintf(dbName, "%s.db", FTW_STR_VARIANTS[C4_variant]) : sprintf(dbName, "%ux%u_%s.db", COLS, ROWS, FTW_STR_VARIANTS[C4_variant]);
    SQLite_open(&database, dbName);
    UI_IS_POP10 ? SQLite_delete(database) : FTW_VOID_NOP;
#else
    ResultRing_reset();
#endif

    if (UI_IS_POPOUT_OR_POP10)
    {
        for (popTokens[COLS_X2] = '\0', i = 0; i < COLS_X2; i++)
        {
            popTokens[i] = i + (i < COLS ? 'A' : 'a' - COLS);
        }

        if (UI_IS_POP10)
        {
            popTokens[COLS_X2] = POP10_PASS_CHR;
            popTokens[COLS_X2 + 1] = '\0';
        }
    }

    bool runLoop = true, showWinMsg = true, solved = false;

    while (runLoop)
    {
        if (showWinMsg)
        {
            switch ((winner = UI_IS_MAKE7 ? Make7_winner(&UI_m7) : Interface_Connect4_winner(&UI_c4, &UI_p10)))
            {
            case 0:
                printf("%s %s\n", FTW_STR_GOOD_GAME, FTW_STR_RESTART_OR_UNDO);
                showWinMsg = false;
                break;
            case 1 ... 2:
                printf(FTW_STR_PLAYER_HAS_WON " %s\n", winner, FTW_STR_RESTART_OR_UNDO);
                showWinMsg = false;
            default:
                break;
            }

            if (HashRing_repeat())
            {
                printf("%s %s\n", FTW_STR_THREE_FOLD, FTW_STR_RESTART_OR_UNDO);
                HashRing_reset();

                winner = 0;
                showWinMsg = false;
            }
        }

        if (UI_IS_MAKE7)
        {
            Make7_moves(&UI_m7) & 1 ? printf("\e[93m" FTW_STR_P2_PROMPT "\e[0m") : printf("\e[92m" FTW_STR_P1_PROMPT "\e[0m");
        }
        else
        {
            Interface_Connect4_turn(UI_c4.plies, UI_p10.turn) ? printf("\e[91m" FTW_STR_P2_PROMPT "\e[0m") : printf("\e[93m" FTW_STR_P1_PROMPT "\e[0m");
        }

        fflush(stdout);

        cmdCnt = REC_gets(&cmd);
        cmdLen = strlen(cmd);

        if (Interface_findShortCmd(cmd, '?', "help"))
        {
            Interface_help();
        }
        else if (Interface_findLongCmd(cmd, "exit") || Interface_findShortCmd(cmd, 'q', "quit") || cmdCnt < 0)
        {
            UI_run = runLoop = false;
        }
        else if (Interface_findShortCmd(cmd, '-', "print"))
        {
            UI_IS_MAKE7 ? Make7_print(&UI_m7) : Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
        }
        else if (Interface_findShortCmd(cmd, 'x', "moves"))
        {
            printf("%" PRIu16 "\n", UI_IS_MAKE7 ? Make7_moves(&UI_m7) : UI_c4.plies);
        }
        else if (UI_IS_POP10 && Interface_findLongCmd(cmd, "pops"))
        {
            Connect4_pop10_print(&UI_p10);
        }
        else if (Interface_findShortCmd(cmd, 'z', "list"))
        {
            if (winner == -1)
            {
                UI_IS_MAKE7 ? Interface_Make7_list(&UI_m7) : Interface_Connect4_list(&UI_c4, &UI_p10, UI_IS_POPOUT, UI_IS_POP10);
            }
        }
        else if (Interface_findShortCmd(cmd, 'y', "hist"))
        {
            if (UI_IS_MAKE7)
            {
                Interface_Make7_moves(&UI_m7);
            }
            else
            {
                Connect4_moves(&UI_c4);
                UI_c4.plies ? putchar('\n') : FTW_VOID_NOP;
            }
        }
        else if (Interface_findLongCmd(cmd, "emoji"))
        {
            if (UI_IS_MAKE7)
            {
                puts(FTW_STR_NO_EMOJI);
            }
            else
            {
                g_emoji = !g_emoji;
                Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
                UI_IS_POP10 ? Connect4_pop10_print(&UI_p10) : FTW_VOID_NOP;
            }
        }
        else if (Interface_findShortCmd(cmd, 's', "solve"))
        {
            if (winner == -1)
            {
#ifdef FTW_SQLITE
                if (UI_IS_MAKE7)
                {
                    m7Key = Make7_partKey(&UI_m7);
                }
                else
                {
                    c4Key = Connect4_key(&UI_c4);
                    ptKey = Connect4_pop10_stateKey(&UI_p10);
                }

                switch (C4_variant)
                {
                default:
                    dbQuery = SQLite_Connect4_query(database, c4Key, &result);
                    break;
                case CONNECT4_POP10:
                    dbQuery = SQLite_Connect4_pop10_query(database, c4Key, ptKey, &result);
                    break;
                case CONNECT4_MAKE7:
                    dbQuery = SQLite_Make7_query(database, m7Key, UI_m7.tile1, UI_m7.tile2, M7_targetMethod, &result);
                    break;
                }

                if (!dbQuery)
                {
#else
                if (UI_IS_MAKE7)
                {
                    m7Key = Make7_canonicalize(Make7_partKey(&UI_m7));
                    t1Key =  Make7_canonicalize(UI_m7.tile1);
                    t2Key =  Make7_canonicalize(UI_m7.tile2);
                    result = ResultRing_query(m7Key, t1Key, t2Key);
                }
                else
                {
                    c4Key = Connect4_canonicalize(Connect4_key(&UI_c4));
                    ptKey = Interface_Connect4_pop10_2ndKey(&UI_p10);
                    result = ResultRing_query(c4Key, ptKey, UINT64_MAX);
                }

                if (result.wdl == NULL_CHAR)
                {
#endif
                    switch (C4_variant)
                    {
                    default:
                        result = NegaScout_Connect4_iterative(&UI_c4, true);
                        break;
                    case CONNECT4_POP10:
                        result = NegaScout_Connect4_pop10_iterative(&UI_c4, &UI_p10, true);
                        break;
                    case CONNECT4_MAKE7:
                        result = NegaScout_Make7_iterative(&UI_m7, true);
                        break;
                    }

                    solved = false;
#ifdef FTW_SQLITE
                    switch (C4_variant)
                    {
                    default:
                        SQLite_Connect4_insert(database, c4Key, &result);
                        break;
                    case CONNECT4_POP10:
                        SQLite_Connect4_pop10_insert(database, c4Key, ptKey, &result);
                        break;
                    case CONNECT4_MAKE7:
                        SQLite_Make7_insert(database, m7Key, t1Key, t2Key, M7_targetMethod, &result);
                        break;
                    }
#else
                    UI_IS_MAKE7 ? ResultRing_insert(m7Key, t1Key, t2Key, result) : ResultRing_insert(c4Key, ptKey, UINT64_MAX, result);
#endif
                }

                Result_print(result, &result, false, true);
                putchar('\a');
            }
        }
        else if (Interface_findShortCmd(cmd, 't', "mcts"))
        {
            if (winner == -1) // TODO: add MCTS for Make 7
            {
                UI_IS_MAKE7 ? puts("Under construction.") : MCTS_Connect4_search(&UI_c4);
            }
        }
        else if (UI_IS_CYLINDER && Interface_findLongCmd(cmd, "<"))
        {
            UI_c4.mask = Connect4_rotateBoardLeft(UI_c4.mask);
            UI_c4.side = Connect4_rotateBoardLeft(UI_c4.side);

            Connect4_rotateHistLeft(&UI_c4);
            Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));

            solved = false;
        }
        else if (UI_IS_CYLINDER && Interface_findLongCmd(cmd, ">"))
        {
            UI_c4.mask = Connect4_rotateBoardRight(UI_c4.mask);
            UI_c4.side = Connect4_rotateBoardRight(UI_c4.side);

            Connect4_rotateHistRight(&UI_c4);
            Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));

            solved = false;
        }
        else if (Interface_findShortCmd(cmd, 'r', "restart"))
        {
            switch (C4_variant)
            {
            default:
                Connect4_reset(&UI_c4);
                break;
            case CONNECT4_POP10:
                Connect4_pop10_reset(&UI_c4, &UI_p10);
                break;
            case CONNECT4_MAKE7:
                Make7_reset(&UI_m7);
                break;
            }

            if (UI_IS_MAKE7)
            {
                Make7_print(&UI_m7);
                UI_m7UndoIdx = 0;
            }
            else
            {
                Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
                UI_IS_POP10 ? Connect4_pop10_print(&UI_p10) : FTW_VOID_NOP;

                HashRing_reset();
                HashRing_push(Connect4_canonicalize(Connect4_key(&UI_c4)));
            }

            showWinMsg = true;
            solved = false;
        }
        else if (Interface_findShortCmd(cmd, 'u', "undo"))
        {
            if (UI_IS_MAKE7)
            {
                if (UI_m7UndoIdx)
                {
                    UI_m7MoveHist[UI_m7UndoIdx--] = 0;
                    UI_m7 = UI_m7UndoBuff[UI_m7UndoIdx];

                    Make7_print(&UI_m7);
                }
            }
            else if (UI_c4.plies)
            {
                UI_IS_POP10 ? Connect4_pop10_unplay(&UI_c4, &UI_p10) : Connect4_unplay(&UI_c4);
                Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
                UI_IS_POP10 ? Connect4_pop10_print(&UI_p10) : FTW_VOID_NOP;
                HashRing_pop();
            }

            showWinMsg = true;
            solved = false;
        }
        else if (Interface_findShortCmd(cmd, 'v', "view"))
        {
            if (winner == -1)
            {
                policyMove = UI_IS_MAKE7 ? Connect4_noMovePolicy(&UI_c4) : Connect4_policy(&UI_c4);

                switch (C4_variant)
                {
                default:
                    bestMove = NegaScout_Connect4_results(&UI_c4, true);
                    break;
                case CONNECT4_POP10:
                    bestMove = NegaScout_Connect4_pop10_results(&UI_c4, &UI_p10, true);
                    break;
                case CONNECT4_MAKE7:
                    NegaScout_Make7_results(&UI_m7, true, m7MovChr);
                    break;
                }

                bestMove = policyMove ? policyMove : bestMove;

                printf("Recommended move: %c%c\n", UI_IS_MAKE7 ? m7MovChr[0] : bestMove, UI_IS_MAKE7 * m7MovChr[1]);
                putchar('\a');

                solved = true;
            }
        }
        else if (Interface_findShortCmd(cmd, 'w', "play"))
        {
            if (winner == -1)
            {
                if (!solved)
                {
                    switch (C4_variant)
                    {
                    default:
                        bestMove = NegaScout_Connect4_results(&UI_c4, false);
                        break;
                    case CONNECT4_POP10:
                        bestMove = NegaScout_Connect4_pop10_results(&UI_c4, &UI_p10, false);
                        break;
                    case CONNECT4_MAKE7:
                        NegaScout_Make7_results(&UI_m7, false, m7MovChr);
                        break;
                    }

                    bestMove = !UI_IS_MAKE7 && (policyMove = Connect4_policy(&UI_c4)) ? policyMove : bestMove;
                }

                switch (C4_variant)
                {
                default:
                    Connect4_parse(&UI_c4, bestMove);
                    break;
                case CONNECT4_POP10:
                    Connect4_pop10_parse(&UI_c4, &UI_p10, bestMove);
                    break;
                case CONNECT4_MAKE7:
                    UI_m7MoveHist[UI_m7UndoIdx] = (m7MovChr[0] - '1') << 3 | (toupper(m7MovChr[1]) - 'A');
                    UI_m7UndoBuff[UI_m7UndoIdx++] = UI_m7;
                    Make7_parse(&UI_m7, m7MovChr);
                    break;
                }

                UI_IS_MAKE7 ? Make7_print(&UI_m7) : Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
                UI_IS_POP10 ? Connect4_pop10_print(&UI_p10) : FTW_VOID_NOP;
                !UI_IS_MAKE7 ? HashRing_push(Connect4_canonicalize(Connect4_key(&UI_c4))) : FTW_VOID_NOP;

                printf("Calculated move: %c%c\n", UI_IS_MAKE7 ? m7MovChr[0] : bestMove, UI_IS_MAKE7 * m7MovChr[1]);
                putchar('\a');

                solved = false;
            }
        }
        else if (!UI_IS_MAKE7 && (arg = REC_strcmd(cmd, "size")))
        {
            if (*arg)
            {
                unsigned long argWidth = 0, argHeight = 0;

                for (uint8_t j = 0; *arg && j < 2;)
                {
                    if (isdigit(*arg))
                    {
                        j++ ? (argHeight = strtoul(arg, &arg, 10)) : (argWidth = strtoul(arg, &arg, 10));
                    }
                    else
                    {
                        arg++;
                    }
                }

                if (argWidth && argHeight)
                {
                    UI_cols = argWidth;
                    UI_rows = argHeight;
                    C4_pop10NotReady = true;
                    runLoop = false;
                }
                else
                {
                    printf("%s size [ <width> <height> ]\n", FTW_STR_USAGE);
                }
            }
            else
            {
                printf("%dx%d\n", COLS, ROWS);
            }
        }
        else if ((arg = REC_strcmd(cmd, "table")))
        {
            if (*arg)
            {
                bool tableNotFinal = true;

                while (arg && isblank(*arg))
                {
                    arg++;
                }

                if (!strcmp(arg, "inc"))
                {
                    UI_ttMulti++;
                    tableNotFinal = false;
                }
                else if (!strcmp(arg, "dec"))
                {
                    if (UI_ttMulti > 1)
                    {
                        UI_ttMulti--;
                        tableNotFinal = false;
                    }
                    else
                    {
                        puts("Cannot decrement the table multiplier below 1.");
                    }
                }

                if (tableNotFinal)
                {
                    printf("%s table [ inc | dec ]\n", FTW_STR_USAGE);
                }
                else
                {
                    UI_ttNotReady = true;
                    runLoop = false;
                }
            }
            else
            {
                printf("%u x %u = %u => %u\n", UI_ttSize, UI_ttMulti, UI_ttSize * UI_ttMulti, TransTable_size(UI_ttSize * UI_ttMulti));
            }

        }
        else if ((arg = REC_strcmd(cmd, "ruleset")))
        {
            if (*arg)
            {
                bool ruleNotFinal = true;

                const size_t STR_VAR_LEN = sizeof(FTW_STR_VARIANTS) / sizeof(*FTW_STR_VARIANTS);

                for (i = 0; ruleNotFinal && i < STR_VAR_LEN; i++)
                {
                    if (REC_strcasecmp(arg, FTW_STR_VARIANTS[i]))
                    {
                        C4_variant = i;
                        ruleNotFinal = runLoop = false;

                        break;
                    }
                }

                if (ruleNotFinal)
                {
                    printf("%s ruleset [ ", FTW_STR_USAGE);

                    for (i = 0; i < STR_VAR_LEN; i++)
                    {
                        printf("<%s> ", FTW_STR_VARIANTS[i]);
                    }

                    puts("]");
                }
                else
                {
                    UI_c4NotReady = C4_pop10NotReady = true;
                }
            }
            else
            {
                switch (C4_variant)
                {
                default:
                    printf("%s %s\n", FTW_STR_CONNECT4, UI_IS_POP10 ? FTW_STR_POP10 : FTW_STR_VARIANTS[C4_variant]);
                    break;
                case CONNECT4_MAKE7:
                    printf("%s\n", FTW_STR_MAKE7);
                    break;
                }
            }
        }
#ifdef FTW_SQLITE
        else if (Interface_findLongCmd(cmd, "delete"))
        {
            char confirm = 'N';

            printf(FTW_STR_CONFIRM);
            fflush(stdout);

            switch ((confirm = toupper(getchar())))
            {
            case 'Y':
                SQLite_delete(database);
            default:
                while (getchar() != '\n');
            case '\n':
                break;
            }
        }
        else if ((arg = REC_strcmd(cmd, "oracle")))
        {
            if (winner == -1)
            {
                if (*arg)
                {
                    unsigned long orDepth = 0;
                    bool orFlavor = true; // false => BFS; true => DFS

                    orDepth = strtoul(arg, &arg, 10);

                    while (arg && isblank(*arg))
                    {
                        arg++;
                    }

                    if (!strcmp(arg, "bfs"))
                    {
                        orFlavor = false;
                    }

                    if (UI_IS_MAKE7)
                    {
                        puts("Coming soon."); // TODO: oracles for Make 7
                    }
                    else
                    {
                        if (!SQLite_Connect4_query(database, Connect4_key(&UI_c4), nullptr))
                        {
                            !UI_IS_POP10 ? (Interface_Connect4_oracle(&UI_c4, orDepth, orFlavor, UI_IS_POPOUT), putchar('\a')) : puts("Move oracles are not supported in Pop 10.");
                        }
                    }
                }
                else
                {
                    printf("%s oracle <depth> [ bfs | dfs ]\n", FTW_STR_USAGE);
                }
            }
        }
        else if ((arg = REC_strcmd(cmd, "insert")))
        {
            bool inserted = false; Result dbRes;

            if (*arg)
            {
                ResultChar dbChar = toupper(*arg);

                switch (dbChar)
                {
                case WIN_CHAR:
                case LOSS_CHAR:
                    dbRes = (Result) { .wdl = dbChar };
                    ++arg && isdigit(*arg) ? dbRes.dtw = strtoll(arg, nullptr, 10), inserted = true : FTW_VOID_NOP;
                    break;
                case DRAW_CHAR:
                    dbRes = (Result) { .wdl = dbChar, .dtw = 0 };
                    ++arg && (isblank(*arg) || !*arg) ? inserted = true : FTW_VOID_NOP;
                default:
                    break;
                }
            }

            if (inserted)
            {
                if (UI_IS_MAKE7)
                {
                    m7Key = Make7_canonicalize(Make7_partKey(&UI_m7));
                    t1Key = Make7_canonicalize(UI_m7.tile1);
                    t2Key = Make7_canonicalize(UI_m7.tile2);
                }
                else
                {
                    c4Key = Connect4_canonicalize(Connect4_key(&UI_c4));
                }

                if (UI_IS_MAKE7 ? SQLite_Make7_query(database, m7Key, t1Key, t2Key, M7_targetMethod, nullptr) : SQLite_Connect4_query(database, c4Key, nullptr))
                {
                    char overwrite = 'N';

                    printf(FTW_STR_OVERWRITE);
                    fflush(stdout);

                    switch ((overwrite = toupper(getchar())))
                    {
                    case 'Y':
                        UI_IS_MAKE7 ? SQLite_Make7_insert(database, m7Key, t1Key, t2Key, M7_targetMethod, &dbRes) : SQLite_Connect4_insert(database, c4Key, &dbRes);
                    default:
                        while (getchar() != '\n');
                    case '\n':
                        break;
                    }
                }
                else
                {
                    UI_IS_MAKE7 ? SQLite_Make7_insert(database, m7Key, t1Key, t2Key, M7_targetMethod, &dbRes) : SQLite_Connect4_insert(database, c4Key, &dbRes);
                }
            }
            else
            {
                printf("%s insert <result>\n", FTW_STR_USAGE);
            }
        }
#endif
        else if ((arg = REC_strcmd(cmd, "perft")))
        {
            bool perftNotFinal = true;

            if (*arg)
            {
                unsigned long perftDepth = strtoul(arg, &arg, 10);

                if (perftDepth <= 999)
                {
                    perftNotFinal = false;

                    Interface_perft(&UI_perft, &UI_c4, &UI_p10, &UI_m7, perftDepth);
                    putchar('\a');
                }
            }

            if (perftNotFinal)
            {
                printf("%s perft <depth 0-999>\n", FTW_STR_USAGE);
            }
        }
        else if (UI_IS_MAKE7 && REC_strcmd(cmd, "tiles"))
        {
            printf("[1] %u [2] %u [3] %u\n", Make7_count(UI_m7.avails, UI_m7.turn, 0), Make7_count(UI_m7.avails, UI_m7.turn, 1), Make7_count(UI_m7.avails, UI_m7.turn, 2));
        }
        else if (UI_IS_MAKE7 && REC_strcmd(cmd, "tgwin"))
        {
#ifndef FTW_SQLITE
            ResultRing_reset();
#endif
            M7_targetMethod = !M7_targetMethod;
            Make7_setTargetMethod();
            printf("Win method has been set to %s.\n", M7_targetMethod ? FTW_STR_WINDOWING : FTW_STR_EXACT);

            showWinMsg = false;
        }
        else if (isdigit(*cmd) || (UI_IS_POPOUT_OR_POP10 && strpbrk(cmd, popTokens)))
        {
            if (winner == -1)
            {
                if (UI_IS_MAKE7)
                {
                    bool Make7_notFinal = true;
                    const char *restrict cmdCur = cmd;

                    UI_m7UndoBuff[UI_m7UndoIdx] = UI_m7;

                    while (cmdCur[0] && cmdCur[1])
                    {
                        if (!Make7_parse(&UI_m7, cmdCur))
                        {
                            Make7_notFinal = true;
                            break;
                        }

                        Make7_notFinal = false;
                        UI_m7MoveHist[UI_m7UndoIdx++] = (cmdCur[0] - '1') << 3 | (toupper(cmdCur[1]) - 'A');
                        UI_m7UndoBuff[UI_m7UndoIdx] = UI_m7;
                        cmdCur += 2;
                    }

                    if (Make7_notFinal)
                    {
                        printf(FTW_STR_S_INVALID, cmdCur, Make7_moves(&UI_m7), FTW_STR_TYPE_Z_LIST);
                    }

                    Make7_print(&UI_m7);
                }
                else
                {
                    for (i = 0; i < cmdLen; i++)
                    {
                        if (UI_IS_POP10 ? !Connect4_pop10_parse(&UI_c4, &UI_p10, cmd[i]) : !Connect4_parse(&UI_c4, cmd[i]))
                        {
                            printf(FTW_STR_C_INVALID, cmd[i], UI_c4.plies, FTW_STR_TYPE_Z_LIST);
                            break;
                        }

                        HashRing_push(Connect4_canonicalize(Connect4_key(&UI_c4)));
                    }

                    Connect4_print(&UI_c4, Interface_Connect4_turn(UI_c4.plies, UI_p10.turn));
                    UI_IS_POP10 ? Connect4_pop10_print(&UI_p10) : FTW_VOID_NOP;
                }

                solved = false;
            }
        }
        else if (cmdLen)
        {
            puts(FTW_STR_DISPLAY_COMMANDS);
        }

        REC_free(cmd);
    }

    TransTable_destroy(&NS_table);
    PathTable_destroy(&NS_path);
    PathStack_destroy(&NS_stack);
    PathGraph_destroy(&NS_graph);
    NegaScout_History_destroy();

    if (!UI_IS_MAKE7)
    {
        Connect4_destroy(&UI_c4);
        Connect4_globals_destroy();
    }

#ifdef FTW_SQLITE
    SQLite_close(database);
#endif
}

#endif // INTERFACE_H //
