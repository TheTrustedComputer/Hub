/*
 *  Author: 2026- TheTrustedComputer
 *
 *  This is a data structure implementation of the popular Connect Four game.
 *  The board is represented by two 64-bit unsigned integers: a player's disk and the union of all disks.
 *  It currently supports the original rules and two official Hasbro variants: PopOut and Pop 10.
 *  It also supports the Misere (avoid winning) and Cylinder (horizontally wrapping board) variants.
 *
 *  Illustration for the 7x6 bitboard:
 *  .. .. .. .. .. .. ..
 *  05 12 19 26 33 40 47
 *  04 11 18 25 32 39 46
 *  03 10 17 24 31 38 45
 *  02 09 16 23 30 37 44
 *  01 08 15 22 29 36 43
 *  00 07 14 21 28 35 42
 *
 *  Numbers indicate the bit index or offset of each possible location for a disk.
 *  Dots are where these bits are unused, yet useful for hashing and preventing wraparound.
 *
 *  Original rules:
 *  - Two players take turns dropping disks from the top of an empty board.
 *  - Players are not allowed to drop a disk into a column that is full.
 *  - Once a disk is on the board, it stays there. The turn switches to the other player.
 *  - A player wins if they have four disks in a row horizontally, vertically, or diagonally.
 *  - If all columns are full and neither player has a four in a row, the game ends in a draw.
 *
 *  Misere rules:
 *  - Original rules apply, except that the winner becomes the loser.
 *
 *  Cylinder rules:
 *  - Same as the original, but the board wraps around horizontally.
 *
 *  PopOut rules:
 *  - The game is played normally, but players are granted an extra move called a pop.
 *  - A pop removes one of the player's bottom disks, causing the disks above it to fall.
 *  - This move introduces the possibility of simultaneous four-in-a-rows and cycles.
 *  - To resolve the first scenario, the player who just moved is declared the winner.
 *  - The second scenario is treated as a draw by threefold repetition.
 *  - However, the search engine considers infinite play to be a draw.
 *
 *  Pop 10 rules:
 *  - To set it up, players fill the bottom row first, continuing until the board is full.
 *  - There is no particular order in which players drop the disks, regardless of color.
 *  - The game begins when one player pops their disks from the bottom of the board.
 *  - If the popped disk is part of a four-in-a-row, the player keeps it and gets another turn.
 *  - Otherwise, if possible, the player must return it to a different column, and the turn switches.
 *  - If a player has no legal moves, they must pass their turn unless they have made some moves.
 *  - The first player to collect ten disks wins; repetition or insufficiency is a draw.
 *
 *  Solvability status for 7x6:
 *  - Original: Solved. The first player wins in 40 plies (half-moves).
 *  - Misere: Solved. The second player wins in 36 plies.
 *  - Cylinder: Solved. The first player wins in 38 plies.
 *  - PopOut: Solved. The first player wins in 20 plies.
 *  - Pop 10: Unsolved. Depends on the starting position.
 */

#ifndef CONNECT4_H
#define CONNECT4_H

// Use an arbitrary precision integer (-DFTW_INT_WIDTH=128), or the default 64 bits.
// We only support GCC and Clang; building with other compilers is at your own risk.
#ifdef FTW_INT_WIDTH
#if FTW_INT_WIDTH < 64
#error FTW_INT_WIDTH must be at least 64.
#endif
#define FTW_C4_MAX_BITS FTW_INT_WIDTH
#if FTW_INT_WIDTH == 128 && defined(__SIZEOF_INT128__)
    typedef __uint128_t Board;
#else
    typedef unsigned _BitInt(FTW_INT_WIDTH) Board;
#endif
#else
#define FTW_C4_MAX_BITS 64
    typedef uint64_t Board;
#endif

#define BOARD(_x) (Board)(_x)

#ifdef FTW_LIBDIVIDE
#ifdef FTW_BRANCHLESS
    static struct libdivide_u64_branchfree_t libdivide_ROWS_P1;
#else
    static struct libdivide_u64_t libdivide_ROWS_P1;
#endif
#endif

#if FTW_C4_MAX_BITS > 64
////////////////////////////////////////////////////////////
/// @brief  Counts trailing zeros in an arbitrary bitboard.
////////////////////////////////////////////////////////////
#define FTW_Connect4_ctz(_WORD) \
static inline unsigned Connect4_ctz(Board _b) \
{ \
    if (!_b) \
    { \
        return _WORD; \
    } \
    \
    unsigned zeros = 0; \
    \
    while (_b > UINT64_MAX) \
    { \
        zeros += 64; \
        _b >>= 64; \
    } \
    \
    return zeros + stdc_trailing_zeros_ull(_b); \
} \

FTW_Connect4_ctz(FTW_INT_WIDTH);

/////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  The population count of an arbitrary bitboard, which returns the number of set bits.
/////////////////////////////////////////////////////////////////////////////////////////////////
static inline unsigned Connect4_popcnt(Board _b)
{
    unsigned ones = 0;

    while (_b)
    {
        ones += stdc_count_ones_ull(_b);
        _b >>= 64;
    }

    return ones;
}
#endif

typedef enum : uint8_t
{
    CONNECT4_ORIGINAL,
    CONNECT4_MISERE,
    CONNECT4_CYLINDER,
    CONNECT4_POPOUT,
    CONNECT4_POP10,
    CONNECT4_MAKE7
}
Variant;

#pragma pack(push, 1)

typedef struct
{
    Board mask, side;       // occupied, player
    uint8_t *restrict hist; // history of moves
    uint16_t plies;         // half-move count
}
Connect4;

typedef struct
{
    uint8_t pops;           // lower 4 bits: P1; upper 4 bits: P2
    bool phase, turn;       // true: pop phase; false: drop phase
}
Connect4_Pop10;

#pragma pack(pop)

#ifdef FTW_XXHASH
    static constexpr size_t C4_SIZE = sizeof(Board) << 1;
    static XXH64_hash_t C4_xxhSeed;
#else
#if FTW_INT_WIDTH > 64 || defined(FTW_TT_128_BITS) || (FTW_INT_WIDTH == 64 && defined(FTW_TT_128_BITS))
    static uint64_t C4_SALT_A;
#endif
#if FTW_INT_WIDTH > 64
    static uint64_t C4_SALT_B;
#endif
#endif

#define Connect4_dropMask(_mask, _BOT, _ALL) (_mask + _BOT) & (_ALL)
#define Connect4_popMask(_side) _side & BOT_MASK

static constexpr uint8_t POP10_4ROW = 0x80;             // Marker for a disk that was part of a four-in-a-row
static constexpr uint8_t POP10_TGT = 10;                // Number of stockpiled disks required to win
static constexpr uint8_t POP10_TGT_M1 = POP10_TGT - 1;  // One less than the target number of disks
static constexpr char POP10_PASS_CHR = '#';             // Character representation of a passing move

static Xoshiro128 g_rng;
static bool g_emoji;

static uint8_t *restrict C4_columnOrder;
static uint8_t *restrict C4_revColOrder;
static Board C4_pop10Init;
static bool C4_pop10NotReady = true;
static Variant C4_variant;

static Board ALL_MASK, BOT_MASK, TOP_MASK, FULL_MASK, COL_MASK, ALL_COL_MASK;
static uint32_t PLY_LENGTH, PLY_LENGTH_P1;
static uint16_t MOVE_BOUNDS, MOVE_SPACE;
static uint8_t COLS, COLS_M1, COLS_M2, COLS_M3, COLS_X2, COLS_X2_P1, MID_COL, BOARD_AREA, POP10_PASS;
static uint8_t ROWS, ROWS_M1, ROWS_P1, ROWS_P2, ROWS_X2, ROWS_X3, ROWS_P1_X2, ROWS_P1_X2_M2, ROWS_P2_X2, ROWS_P1_X3, ROWS_P2_X2, ROWS_P2_X3;
static uint8_t CM2_X_RP1_P2, CM1_X_RP1_P1, CM2_X_RP1_M2, CM1_X_RP1_M1, CM3_X_RP1_P3, CM3_X_RP1_M3;

static Board Connect4_pop10_fourInARow(const Board, const Board);
static void (*Connect4_play)(Connect4 *const restrict, const uint8_t);
static void (*Connect4_unplay)(Connect4 *const restrict);
static bool (*Connect4_playable)(const Connect4 *const restrict, const uint8_t);
static void (*Connect4_moves)(const Connect4 *const restrict);
static uint8_t (*Connect4_sequence)(const uint8_t);
static void (*Connect4_generate)(const Connect4 *const restrict, uint8_t[restrict static 4], uint8_t *const restrict);
static void (*Connect4_generateAll)(const Connect4 *const restrict, uint8_t[restrict static 4], uint8_t *const restrict);
static void (*Connect4_genNonLosing)(const Connect4 *const restrict, uint8_t[restrict static 4], uint8_t *const restrict);
static bool (*Connect4_fourInARow)(const Board);
static Board (*Connect4_fourInARow_threats)(const Board);
static bool (*Connect4_over)(const Connect4 *const restrict);
static bool (*Connect4_canWin)(const Connect4 *const restrict);
static char (*Connect4_policy)(const Connect4 *const restrict);
static int (*Connect4_winner)(const Connect4 *const restrict);
static Board (*Connect4_canonicalize)(Board);
static bool (*Connect4_parse)(Connect4 *const restrict, const char);

/////////////////////////////////////////////////////////
/// @brief          Prepares Connect 4 global variables.
/// @param  _cols   The number of columns or width.
/// @param  _rows   The number of rows or height.
/////////////////////////////////////////////////////////
static inline void Connect4_prepare(uint8_t _cols, uint8_t _rows)
{
    if (_cols < 4)
    {
        _cols = 4;
    }
    else if (_cols > 16)
    {
        fprintf(stderr, "\e[1m%s: The board size must not exceed 16 columns.\e[0m\n", FTW_STR_ERROR_PREFIX);

        return;
    }

    if (_rows < 4)
    {
        _rows = 4;
    }

    if (_cols * (_rows + 1) > FTW_C4_MAX_BITS)
    {
        fprintf(stderr, "\e[1m%s: The board size is too large for %u bits.\e[0m\n", FTW_STR_ERROR_PREFIX, FTW_C4_MAX_BITS);

        return;
    }

    COLS = _cols;
    COLS_M1 = COLS - 1;
    COLS_M2 = COLS - 2;
    COLS_M3 = COLS - 3;
    COLS_X2 = COLS * 2;
    COLS_X2_P1 = COLS_X2 + 1;
    MID_COL = COLS / 2;
    ROWS = _rows;
    ROWS_M1 = ROWS - 1;
    ROWS_P1 = ROWS + 1;
    ROWS_P2 = ROWS + 2;
    ROWS_X2 = ROWS * 2;
    ROWS_X3 = ROWS * 3;
    ROWS_P1_X2 = ROWS_P1 * 2;
    ROWS_P1_X2_M2 = ROWS_P1_X2 - 2;
    ROWS_P2_X2 = ROWS_P1_X2 + 2;
    ROWS_P1_X3 = ROWS_P1 * 3;
    ROWS_P2_X2 = ROWS_P2 * 2;
    ROWS_P2_X3 = ROWS_P2 * 3;
    CM2_X_RP1_P2 = COLS_M2 * ROWS_P1 + 2;
    CM1_X_RP1_P1 = COLS_M1 * ROWS_P1 + 1;
    CM2_X_RP1_M2 = COLS_M2 * ROWS_P1 - 2;
    CM1_X_RP1_M1 = COLS_M1 * ROWS_P1 - 1;
    CM3_X_RP1_P3 = COLS_M3 * ROWS_P1 + 3;
    CM3_X_RP1_M3 = COLS_M3 * ROWS_P1 - 3;
    BOARD_AREA = COLS * ROWS;
    PLY_LENGTH = BOARD_AREA;
    PLY_LENGTH_P1 = PLY_LENGTH + 1;
    MOVE_BOUNDS = COLS;
    MOVE_SPACE = MOVE_BOUNDS;

    ALL_MASK = BOARD(~0) >> (sizeof(Board) * 8 - (BOARD_AREA + COLS));
    ALL_COL_MASK = (BOARD(1) << ROWS_P1) - 1;
    BOT_MASK = ALL_MASK / ALL_COL_MASK;
    FULL_MASK = BOT_MASK << ROWS;
    TOP_MASK = FULL_MASK >> 1;
    COL_MASK = ALL_COL_MASK >> 1;
    ALL_MASK ^= FULL_MASK;

    if (C4_variant == CONNECT4_POPOUT || C4_variant == CONNECT4_POP10)
    {
        MOVE_BOUNDS = COLS_X2 - (COLS >> 2);
        MOVE_SPACE = COLS_X2;
        C4_variant == CONNECT4_POP10 ? (POP10_PASS = MOVE_SPACE++) : FTW_VOID_NOP;
        PLY_LENGTH = UINT16_MAX;
        PLY_LENGTH_P1 = PLY_LENGTH + 1;
    }
}

//////////////////////////////////////////////////
/// @brief  Sets the Connect 4 column move order.
//////////////////////////////////////////////////
static inline void Connect4_setColMoveOrder(void)
{
    uint8_t i;

    for (i = 0; i < COLS; i++)
    {
        C4_columnOrder[i] = Connect4_sequence(i);
    }

    for (i = 0; i < COLS; i++)
    {
        C4_revColOrder[C4_columnOrder[i]] = i;
    }
}

////////////////////////////////////////////////////////////////////////////////////
/// @brief  Initializes global variables: column move order, libdivide tables, etc.
////////////////////////////////////////////////////////////////////////////////////
static inline void Connect4_globals_init(void)
{
    C4_columnOrder = REC_calloc(COLS, sizeof(*C4_columnOrder), "Could not allocate memory for the column move order.", true);
    C4_revColOrder = REC_calloc(COLS, sizeof(*C4_revColOrder), "Could not allocate memory for the reverse column order.", true);

#ifdef FTW_LIBDIVIDE
#ifdef FTW_BRANCHLESS
    libdivide_ROWS_P1 = libdivide_u64_branchfree_gen(ROWS_P1);
#else
    libdivide_ROWS_P1 = libdivide_u64_gen(ROWS_P1);
#endif
#endif

#ifdef FTW_TT_128_BITS
    {
        Xoshiro256 xsr256; Xoshiro256_init(&xsr256);
#ifdef FTW_XXHASH
        C4_xxhSeed = Xoshiro256ss_next(&xsr256);
#else
        C4_SALT_A = Xoshiro256ss_next(&xsr256);
#if FTW_C4_MAX_BITS > 64
        C4_SALT_B = Xoshiro256ss_next(&xsr256);
#endif
#endif
    }
#endif

    Connect4_setColMoveOrder();
}

////////////////////////////////////////////////////////////////////
/// @brief  Destroys memory allocated by `Connect4_globals_init()`.
////////////////////////////////////////////////////////////////////
static inline void Connect4_globals_destroy(void)
{
    REC_free(C4_columnOrder);
    REC_free(C4_revColOrder);
}

//////////////////////////////////////////////////////////////
/// @brief  Resets a Connect 4 game to the starting position.
/// @param  _c4
//////////////////////////////////////////////////////////////
static inline void Connect4_reset(Connect4 *const restrict _c4)
{
    _c4->mask = _c4->side = _c4->plies = 0;
}

//////////////////////////////////////////////////////////
/// @brief  Resets a Pop 10 game to the initial position.
/// @param  _c4
/// @param  _p10
//////////////////////////////////////////////////////////
static inline void Connect4_pop10_reset(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    if (C4_pop10NotReady)
    {
        uint8_t i, colArr[COLS];

        for (i = 0; i < COLS; i++)
        {
            colArr[i] = i;
        }

        for (i = 0; i < ROWS; i++)
        {
            for (uint8_t j = COLS; j;)
            {
                const uint8_t RND_COL = Xoshiro128pp_nextN(&g_rng, j);
                const uint8_t SWP_COL = colArr[RND_COL];

                _c4->side ^= _c4->mask;
                _c4->mask |= _c4->mask + (BOARD(1) << ROWS_P1 * SWP_COL);

                colArr[RND_COL] = colArr[--j];
                colArr[j] = SWP_COL;
            }
        }

        C4_pop10Init = _c4->side;
        C4_pop10NotReady = false;
    }
    else
    {
        _c4->side = C4_pop10Init;
        _c4->mask = ALL_MASK;
    }

    _p10->pops = _c4->plies = 0;
    _p10->phase = true;
    _p10->turn = false;
}

////////////////////////////////////////////////////
/// @brief      Initializes a Connect 4 game state.
/// @param      _c4
/// @attention  Invoke `Connect4_prepare()` first.
////////////////////////////////////////////////////
static inline void Connect4_init(Connect4 *const restrict _c4)
{
    _c4->hist = REC_calloc(PLY_LENGTH, sizeof(*_c4->hist), "Could not allocate memory for the move history.", true);

    Connect4_reset(_c4);
}

////////////////////////////////////////////////////////////////
/// @brief  Copies the contents pointed to by the move history.
/// @param  _SRC
/// @param  _dst
////////////////////////////////////////////////////////////////
static inline void Connect4_copyHist(const Connect4 *const restrict _SRC, Connect4 *const restrict _dst)
{
    memcpy(_dst->hist, _SRC->hist, sizeof(*_dst->hist) * _SRC->plies);
}

////////////////////////////////////////////////
/// @brief  Deep copies a Connect 4 game state.
/// @param  _SRC
/// @param  _dst
////////////////////////////////////////////////
static inline void Connect4_copy(const Connect4 *const restrict _SRC, Connect4 *const restrict _dst)
{
    _dst->mask = _SRC->mask;
    _dst->side = _SRC->side;
    _dst->plies = _SRC->plies;

    Connect4_copyHist(_SRC, _dst);
}

//////////////////////////////////////////////////////
/// @brief  Clones a Connect 4 game state to another.
/// @param  _SRC
/// @param  _dst
//////////////////////////////////////////////////////
static inline void Connect4_clone(const Connect4 *const restrict _SRC, Connect4 *const restrict _dst)
{
    memcpy(_dst, _SRC, sizeof(*_dst));

    _dst->hist = REC_calloc(PLY_LENGTH, sizeof(*_dst->hist), "Could not allocate memory to clone the move history.", true);

    Connect4_copyHist(_SRC, _dst);
}

///////////////////////////////////////////////////////////////
/// @brief  Destroys a Connect 4 game state by freeing memory.
/// @param  _c4
///////////////////////////////////////////////////////////////
static inline void Connect4_destroy(Connect4 *const restrict _c4)
{
    REC_free(_c4->hist);
}

//////////////////////////////////////////////////////////////
/// @brief  Prints a Connect 4 game state to standard output.
/// @param  _c4
/// @param  _TURN
//////////////////////////////////////////////////////////////
static inline void Connect4_print(const Connect4 *const restrict _c4, const bool _TURN)
{
    for (uint8_t i = ROWS; i--;)
    {
        for (uint8_t j = 0; j < COLS; j++)
        {
            const Board CELL = (BOARD(1) << ROWS_P1 * j) * (BOARD(1) << i);

            if (_c4->side & CELL)
            {
                g_emoji ? (_TURN ? printf(FTW_C4_EMOJI_RED_CIRCLE) : printf(FTW_C4_EMOJI_YELLOW_CIRCLE))
                        : (_TURN ? printf(FTW_C4_ASCII_RED_CIRCLE) : printf(FTW_C4_ASCII_YELLOW_CIRCLE));
            }
            else if (_c4->mask & CELL)
            {
                g_emoji ? (_TURN ? printf(FTW_C4_EMOJI_YELLOW_CIRCLE) : printf(FTW_C4_EMOJI_RED_CIRCLE))
                        : (_TURN ? printf(FTW_C4_ASCII_YELLOW_CIRCLE) : printf(FTW_C4_ASCII_RED_CIRCLE));
            }
            else
            {
                g_emoji ? printf(FTW_C4_EMOJI_BLUE_CIRCLE) : printf(FTW_C4_ASCII_BLUE_CIRCLE);
            }
        }

        if (i)
        {
            putchar('\n');
        }
    }

    putchar('\n');
}

//////////////////////////////////////////////////////
/// @brief  Prints a Pop 10 state to standard output.
/// @param  _p10
//////////////////////////////////////////////////////
static inline void Connect4_pop10_print(const Connect4_Pop10 *const restrict _p10)
{
    printf("%u %u\n", _p10->pops & 0xf, _p10->pops >> 4);
}

//////////////////////////////////////////////////
/// @brief  Prints the moves of a Connect 4 game.
/// @param  _C4
//////////////////////////////////////////////////
static inline void Connect4_original_moves(const Connect4 *const restrict _C4)
{
    for (uint16_t i = 0; i < _C4->plies; i++)
    {
        const uint8_t H_COL = _C4->hist[i];

        COLS <= 10 ? printf("%c", H_COL == 10 ? '0' : H_COL + '1') : printf("%c", H_COL + 'A');
    }
}

///////////////////////////////////////////////////////////
/// @brief  PopOut version of `Connect4_original_moves()`.
/// @param  _C4
///////////////////////////////////////////////////////////
static inline void Connect4_popout_moves(const Connect4 *const restrict _C4)
{
    for (uint16_t i = 0; i < _C4->plies; i++)
    {
        const uint8_t H_COL = _C4->hist[i] & ~POP10_4ROW;

        if (COLS <= 10)
        {
            H_COL < COLS ? printf("%c", H_COL == 10 ? '0' : H_COL + '1') : printf("%c", H_COL == POP10_PASS ? POP10_PASS_CHR : H_COL + 'A' - COLS);
        }
        else
        {
            H_COL < COLS ? printf("%c", H_COL + 'A') : printf("%c", H_COL == POP10_PASS ? POP10_PASS_CHR : H_COL + 'a' - COLS);
        }
    }
}

//////////////////////////////////////////////////////////
/// @brief      Appends a move to the array and sorts it.
/// @param _arr Move array.
/// @param _num Move count.
/// @param _MOV Move to add.
/// @param _POS Array position.
/// @param _RNK Move sort rank.
//////////////////////////////////////////////////////////
static inline void Connect4_append(uint8_t _arr[restrict static 4], uint8_t *const restrict _num, const uint8_t _MOV, const uint8_t _POS, const uint8_t _RNK)
{
    _arr[(*_num)++] = _MOV;

    for (uint8_t i = *_num - _POS; --i;)
    {
        uint8_t *const restrict curr = &_arr[i + _POS], *const restrict prev = curr - 1;

        if (C4_revColOrder[*curr - _RNK] < C4_revColOrder[*prev - _RNK])
        {
            const uint8_t SWAP = *curr;

            *curr = *prev;
            *prev = SWAP;
        }
        else
        {
            return;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
/// @brief  Appends and sorts a move into the array depending on `_SORT`.
/// @param  _arr
/// @param  _num
/// @param  _MOV
/// @param  _POS
/// @param  _WND
/// @param  _SWP Whether to swap the move.
//////////////////////////////////////////////////////////////////////////
static inline void Connect4_pop10_append(uint8_t _arr[restrict static 4], uint8_t *const restrict _num, const uint8_t _MOV, const uint8_t _POS, const uint8_t _WND, const bool _SWP)
{
    _arr[(*_num)++] = _MOV;

    for (uint8_t i = *_num - _POS; --i > _WND;)
    {
        uint8_t *const restrict curr = &_arr[i + _POS], *const restrict prev = curr - 1;

        if (_SWP)
        {
            const uint8_t SWAP = *curr;

            *curr = *prev;
            *prev = SWAP;
        }
        else
        {
            return;
        }
    }
}

//////////////////////////////////////////////////////
/// @brief  Drops a piece into a zero-indexed column.
/// @param  _c4
/// @param  _COL
//////////////////////////////////////////////////////
static inline void Connect4_drop(Connect4 *const restrict _c4, const uint8_t _COL)
{
    _c4->side ^= _c4->mask;
    _c4->mask |= _c4->mask + (BOARD(1) << ROWS_P1 * _COL);
    _c4->hist[_c4->plies++] = _COL;
}

//////////////////////////////////////////
/// @brief  Undos the last dropped piece.
/// @param  _c4
//////////////////////////////////////////
static inline void Connect4_undrop(Connect4 *const restrict _c4)
{
    const Board DROP_COL = _c4->mask & COL_MASK << ROWS_P1 * _c4->hist[--_c4->plies];

    _c4->mask ^= DROP_COL & ~(DROP_COL >> 1);
    _c4->side ^= _c4->mask;
}

//////////////////////////////////////////////
/// @brief  Pop 10 form of `Connect4_drop()`.
/// @param  _c4
/// @param  _p10
/// @param  _COL
//////////////////////////////////////////////
static inline void Connect4_pop10_drop(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const uint8_t _COL)
{
    Connect4_drop(_c4, _COL);

    _p10->pops -= 1 << (_p10->turn << 2);
    _p10->turn ^= 1;
    _p10->phase = true;
}

////////////////////////////////////////////////
/// @brief  Pop 10 form of `Connect4_undrop()`.
/// @param  _c4
/// @param  _p10
////////////////////////////////////////////////
static inline void Connect4_pop10_undrop(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    Connect4_undrop(_c4);

    _p10->pops += 1 << ((_p10->turn ^= 1) << 2);
    _p10->phase = false;
}

/////////////////////////////////////////////
/// @brief  Pops a disk from the bottom row.
/// @param  _c4
/// @param  _COL
/////////////////////////////////////////////
static inline void Connect4_pop(Connect4 *const restrict _c4, const uint8_t _COL)
{
    const Board FALL_MASK = _c4->mask & COL_MASK << ROWS_P1 * _COL;

    _c4->mask ^= FALL_MASK & ~(FALL_MASK >> 1);
    _c4->side = _c4->mask ^ (((_c4->side & ~FALL_MASK) | (_c4->side & FALL_MASK) >> 1) & ALL_MASK);
    _c4->hist[_c4->plies++] = _COL + COLS;
}

////////////////////////////////////////
/// @brief  Undos the last popped disk.
/// @param  _c4
////////////////////////////////////////
static inline void Connect4_unpop(Connect4 *const restrict _c4)
{
    const uint8_t LAST_POP = _c4->hist[--_c4->plies] - COLS;
    const Board COL_INDEX = COL_MASK << ROWS_P1 * LAST_POP;
    const Board FALL_MASK = _c4->mask & COL_INDEX;

    _c4->side = ((_c4->side & ~FALL_MASK) | (_c4->side & FALL_MASK) << 1) ^ (_c4->mask |= COL_INDEX & (_c4->mask + BOT_MASK));
}

////////////////////////////////////////////////
/// @brief  Pops a disk under the Pop 10 rules.
/// @param  _c4
/// @param  _p10
/// @param  _COL
////////////////////////////////////////////////
static inline void Connect4_pop10_pop(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const uint8_t _COL)
{
    const Board POP_BIT = ROWS_P1 * _COL;
    const Board FALL_MASK = _c4->mask & COL_MASK << POP_BIT;
    const bool POP_CONNECT = Connect4_pop10_fourInARow(_c4->side, BOARD(1) << POP_BIT);

    _c4->mask ^= FALL_MASK & ~(FALL_MASK >> 1);
    _c4->side = ((_c4->side & ~FALL_MASK) | (_c4->side & FALL_MASK) >> 1) & ALL_MASK;
    _c4->hist[_c4->plies++] = (_COL + COLS) | (POP_CONNECT * POP10_4ROW);

    _p10->phase = POP_CONNECT;
    _p10->pops += 1 << (_p10->turn << 2);
}

///////////////////////////////////////////////////
/// @brief  The reverse of `Connect4_pop10_pop()`.
/// @param  _c4
/// @param  _p10
///////////////////////////////////////////////////
static inline void Connect4_pop10_unpop(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    uint8_t lastPop = _c4->hist[--_c4->plies];

    const uint8_t CONNECT_BIT = lastPop & ~POP10_4ROW;

    (_p10->phase = CONNECT_BIT) ? (lastPop = CONNECT_BIT - COLS) : FTW_VOID_NOP;
    _p10->pops -= 1 << (_p10->turn << 2);

    const Board COL_OFFSET = ROWS_P1 * lastPop;
    const Board COL_INDEX = COL_MASK << COL_OFFSET;
    const Board FALL_MASK = _c4->mask & COL_INDEX;

    _c4->side = ((_c4->side & ~FALL_MASK) | (_c4->side & FALL_MASK) << 1) | BOARD(1) << COL_OFFSET;
    _c4->mask |= COL_INDEX & (_c4->mask + BOT_MASK);
}

/////////////////////////////////////////////////////////
/// @brief  Passes the turn to the opponent if possible.
/// @param  _c4
/// @param  _p10
/////////////////////////////////////////////////////////
static inline void Connect4_pop10_pass(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    _c4->side ^= _c4->mask;
    _p10->turn ^= 1;
}

///////////////////////////////////////////////
/// @brief  Performs a passing move in Pop 10.
/// @param  _c4
/// @param  _p10
///////////////////////////////////////////////
static inline void Connect4_pop10_passMove(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    Connect4_pop10_pass(_c4, _p10);

    _c4->hist[_c4->plies++] = POP10_PASS;
}

//////////////////////////////////////////////
/// @brief  Performs the converse of passing.
/// @param  _c4
/// @param  _p10
//////////////////////////////////////////////
static inline void Connect4_pop10_unpassMove(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    Connect4_pop10_pass(_c4, _p10);

    _c4->plies--;
}

////////////////////////////////////////////////////
/// @brief  Is the player forced to pass in Pop 10?
/// @param  _C4
/// @param  _P10
/// @return `true` if yes; otherwise `false`.
////////////////////////////////////////////////////
static inline bool Connect4_pop10_passForced(const Connect4 *const restrict _C4, const Connect4_Pop10 *const restrict _P10)
{
    return _P10->phase && !(Connect4_popMask(_C4->side));
}

/////////////////////////////////////////////////
/// @brief  Plays a move under the PopOut rules.
/// @param  _c4
/// @param  _COL
/////////////////////////////////////////////////
static inline void Connect4_popout_play(Connect4 *const restrict _c4, const uint8_t _COL)
{
    _COL < COLS ? Connect4_drop(_c4, _COL) : Connect4_pop(_c4, _COL - COLS);
}

/////////////////////////////////////////////////////
/// @brief  The inverse of `Connect4_popout_play()`.
/// @param  _c4
/////////////////////////////////////////////////////
static inline void Connect4_popout_unplay(Connect4 *const restrict _c4)
{
    _c4->hist[_c4->plies - 1] < COLS ? Connect4_undrop(_c4) : Connect4_unpop(_c4);
}

///////////////////////////////////////////////////
/// @brief  Makes a move according to Pop10 rules.
/// @param  _c4
/// @param  _p10
/// @param  _COL
///////////////////////////////////////////////////
static inline void Connect4_pop10_play(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const uint8_t _COL)
{
    _COL == POP10_PASS ? Connect4_pop10_passMove(_c4, _p10) : (_COL < COLS ? Connect4_pop10_drop(_c4, _p10, _COL) : Connect4_pop10_pop(_c4, _p10, _COL - COLS));
}

////////////////////////////////////////////////////
/// @brief  The reverse of `Connect4_pop10_play()`.
/// @param  _c4
/// @param  _p10
////////////////////////////////////////////////////
static inline void Connect4_pop10_unplay(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10)
{
    const uint8_t REC_COL = _c4->hist[_c4->plies - 1];

    REC_COL == POP10_PASS ? Connect4_pop10_unpassMove(_c4, _p10) : (REC_COL < COLS ? Connect4_pop10_undrop(_c4, _p10) : Connect4_pop10_unpop(_c4, _p10));
}

//////////////////////////////////////////////////////
/// @brief  Is the given column droppable (not full)?
/// @param  _c4
/// @param  _COL
/// @return `true` if permitted; otherwise `false`.
//////////////////////////////////////////////////////
static inline bool Connect4_droppable(const Connect4 *const restrict _C4, const uint8_t _COL)
{
    return !(ALL_COL_MASK << ROWS_P1 * _COL & Connect4_dropMask(_C4->mask, BOT_MASK, FULL_MASK));
}

//////////////////////////////////////////////////
/// @brief  Is the piece at this column poppable?
/// @param  _c4
/// @param  _COL
/// @return `true` if allowed; otherwise `false`.
//////////////////////////////////////////////////
static inline bool Connect4_poppable(const Connect4 *const restrict _C4, const uint8_t _COL)
{
    return _C4->side & BOARD(1) << ROWS_P1 * _COL & BOT_MASK;
}

//////////////////////////////////////////////
/// @brief  Is a move playable in PopOut?
/// @param  _C4
/// @param  _COL
/// @return `true` if yes; otherwise `false`.
//////////////////////////////////////////////
static inline bool Connect4_popout_playable(const Connect4 *const restrict _C4, const uint8_t _COL)
{
    return _COL < COLS ? Connect4_droppable(_C4, _COL) : Connect4_poppable(_C4, _COL - COLS);
}

////////////////////////////////////////////////////////
/// @brief      Checks if a player has a four in a row.
/// @param  _S  A bitboard of the side/player to move.
/// @return     `true` if yes; otherwise `false`.
////////////////////////////////////////////////////////
static inline bool Connect4_original_fourInARow(const Board _S)
{
    const Board VER = _S & _S >> 1, HOR = _S & _S >> ROWS_P1, DI1 = _S & _S >> ROWS, DI2 = _S & _S >> ROWS_P2;

    return (VER & VER >> 2) || (HOR & HOR >> ROWS_P1_X2) || (DI1 & DI1 >> ROWS_X2) || (DI2 & DI2 >> ROWS_P2_X2);
}

////////////////////////////////////////////////////////////////////////
/// @brief  Checks for wraparound four-in-a-rows in Cylinder Connect 4.
////////////////////////////////////////////////////////////////////////
static inline bool Connect4_cylinder_fourInARow(const Board _S)
{
    const Board HOR_13 = _S & _S >> ROWS_P1 * COLS_M3 & _S >> ROWS_P1 * COLS_M2 & _S >> ROWS_P1 * COLS_M1;
    const Board HOR_22 = _S & _S >> ROWS_P1 & _S >> ROWS_P1 * COLS_M2 & _S >> ROWS_P1 * COLS_M1;
    const Board HOR_31 = _S & _S >> ROWS_P1 & _S >> ROWS_P1_X2 & _S >> ROWS_P1 * COLS_M1;

    const Board DI1_13 = _S & _S >> CM3_X_RP1_P3 & _S >> CM2_X_RP1_P2 & _S >> CM1_X_RP1_P1;
    const Board DI1_22 = _S & _S >> CM2_X_RP1_P2 & _S >> CM1_X_RP1_P1 & _S >> ROWS;
    const Board DI1_31 = _S & _S >> CM1_X_RP1_P1 & _S >> ROWS & _S >> ROWS_P1_X2_M2;

    const Board D12_13 = _S & _S >> CM3_X_RP1_M3 & _S >> CM2_X_RP1_M2 & _S >> CM1_X_RP1_M1;
    const Board D12_22 = _S & _S >> CM2_X_RP1_M2 & _S >> CM1_X_RP1_M1 & _S >> ROWS_P2;
    const Board D12_31 = _S & _S >> CM1_X_RP1_M1 & _S >> ROWS_P2 & _S >> ROWS_P2_X2;

    return Connect4_original_fourInARow(_S) || HOR_13 || HOR_22 || HOR_31 || DI1_13 || DI1_22 || DI1_31 || D12_13 || D12_22 || D12_31;
}

//////////////////////////////////////////////////////////////////////////////////
/// @brief  A `Connect4_original_fourInARow()` that does not check vertical wins.
/// @param  _S
/// @note   PopOut optimization where pop moves only decrease the column height.
//////////////////////////////////////////////////////////////////////////////////
static inline bool Connect4_popout_fourInARow(const Board _S)
{
    const Board HOR = _S & _S >> ROWS_P1, DI1 = _S & _S >> ROWS, DI2 = _S & _S >> ROWS_P2;

    return (HOR & HOR >> ROWS_P1_X2) || (DI1 & DI1 >> ROWS_X2) || (DI2 & DI2 >> ROWS_P2_X2);
}

////////////////////////////////////////////////////////////////////////////
/// @brief      Determines if there are four disks touching the bottom row.
/// @param  _S  Bitboard representing the side to move.
/// @param  _T  Bitboard representing the target disk.
/// @note       Used for the Pop 10 variant if it meets this criterion.
////////////////////////////////////////////////////////////////////////////
static inline Board Connect4_pop10_fourInARow(const Board _S, const Board _T)
{
    const Board VER = _S & _S >> 1, HO1 = _S & _S >> ROWS_P1, HO2 = HO1 & HO1 >> ROWS_P1_X2, DI1 = _S & _S >> ROWS, DI2 = _S & _S >> ROWS_P2;

    return _T & ((VER & VER >> 2) | HO2 | HO2 << ROWS_P1 | HO2 << ROWS_P1_X2 | HO2 << ROWS_P1_X3 | (DI1 & DI1 >> ROWS_X2) << ROWS_X3 | (DI2 & DI2 >> ROWS_P2_X2));
}

///////////////////////////////////////////////////////////////
/// @brief      Masks every open-ended three-in-a-row threats.
/// @param  _S  Bitboard of the side or player owned pieces.
/// @return     Bitboard of potential winnning/blocking moves.
/// @attention  Also retrieves unreachable floating bits.
///////////////////////////////////////////////////////////////
static inline Board Connect4_original_fourInARow_threats(const Board _S)
{
    // Vertical
    Board m = _S << 1 & _S << 2 & _S << 3;

    // Horizontal
    Board n = _S << ROWS_P1 & _S << ROWS_P1_X2;
    m |= (n & _S >> ROWS_P1) | (n & _S << ROWS_P1_X3);
    n >>= ROWS_P1_X3;
    m |= (n & _S << ROWS_P1) | (n & _S >> ROWS_P1_X3);

    // NWSE Diagonal
    n = _S << ROWS & _S << ROWS_X2;
    m |= (n & _S >> ROWS) | (n & _S << ROWS_X3);
    n >>= ROWS_X3;
    m |= (n & _S << ROWS) | (n & _S >> ROWS_X3);

    // NESW Diagonal
    n = _S << ROWS_P2 & _S << ROWS_P2_X2;
    m |= (n & _S >> ROWS_P2) | (n & _S << ROWS_P2_X3);
    n >>= ROWS_P2_X3;
    m |= (n & _S << ROWS_P2) | (n & _S >> ROWS_P2_X3);

    return m; // We AND with a drop mask later, so `m & ~mask & ALL_MASK` does nothing
}

////////////////////////////////////////////////////////////////////////
/// @brief  The cylindrical version of `Connect4_fourInARow_threats()`.
/// @param  _S
////////////////////////////////////////////////////////////////////////
static inline Board Connect4_cylinder_fourInARow_threats(const Board _S)
{
    const uint8_t CM1_X_RP1 = ROWS_P1 * COLS_M1;
    const uint8_t CM2_X_RP1 = ROWS_P1 * COLS_M2;
    const uint8_t CM3_X_RP1 = ROWS_P1 * COLS_M3;

    // HORI
    Board m = _S >> CM1_X_RP1 & _S >> CM2_X_RP1 & _S >> CM3_X_RP1;
    m |= _S << CM1_X_RP1 & _S << CM2_X_RP1 & _S << CM3_X_RP1;
    m |= _S >> ROWS_P1 & _S >> CM1_X_RP1 & _S >> CM2_X_RP1;
    m |= _S << ROWS_P1 & _S << CM1_X_RP1 & _S << CM2_X_RP1;
    m |= _S >> ROWS_P1 & _S >> ROWS_P1_X2 & _S >> CM1_X_RP1;
    m |= _S << ROWS_P1 & _S << ROWS_P1_X2 & _S << CM1_X_RP1;
    m |= _S << ROWS_P1 & _S >> ROWS_P1 & _S >> CM2_X_RP1;
    m |= _S >> ROWS_P1 & _S << ROWS_P1 & _S << CM2_X_RP1;

    Board n = _S & _S >> ROWS_P1 & _S << CM2_X_RP1;
    m |= n >> CM3_X_RP1 | n >> ROWS_P1;
    n = _S & _S << ROWS_P1 & _S >> CM2_X_RP1;
    m |= n << CM3_X_RP1 | n << ROWS_P1;

    // NWSE
    m |= _S >> ROWS & _S >> ROWS_P1_X2_M2 & _S << CM3_X_RP1_P3;
    m |= _S << ROWS & _S >> ROWS & _S << CM2_X_RP1_P2;
    m |= _S << ROWS_P1_X2_M2 & _S << ROWS & _S << CM1_X_RP1_P1;
    m |= _S >> CM3_X_RP1_P3 & _S >> CM2_X_RP1_P2 & _S >> CM1_X_RP1_P1;
    m |= _S >> ROWS & _S << CM2_X_RP1_P2 & _S << CM3_X_RP1_P3;
    m |= _S << ROWS & _S << CM1_X_RP1_P1 & _S << CM2_X_RP1_P2;
    m |= _S >> CM2_X_RP1_P2 & _S >> CM1_X_RP1_P1 & _S >> ROWS;
    m |= _S >> CM3_X_RP1_P3 & _S >> CM2_X_RP1_P2 & _S << ROWS;
    m |= _S << CM1_X_RP1_P1 & _S << CM2_X_RP1_P2 & _S << CM3_X_RP1_P3;
    m |= _S >> CM1_X_RP1_P1 & _S >> ROWS & _S >> ROWS_P1_X2_M2;
    m |= _S >> CM2_X_RP1_P2 & _S << ROWS & _S >> ROWS;
    m |= _S >> CM3_X_RP1_P3 & _S << ROWS_P1_X2_M2 & _S << ROWS;

    // NESW
    m |= _S >> CM3_X_RP1_M3 & _S >> CM2_X_RP1_M2 & _S >> CM1_X_RP1_M1;
    m |= _S << ROWS_P2_X2 & _S << ROWS_P2 & _S << CM1_X_RP1_M1;
    m |= _S << ROWS_P2 & _S >> ROWS_P2 & _S << CM2_X_RP1_M2;
    m |= _S >> ROWS_P2 & _S >> ROWS_P2_X2 & _S << CM3_X_RP1_M3;
    m |= _S >> CM3_X_RP1_M3 & _S >> CM2_X_RP1_M2 & _S << ROWS_P2;
    m |= _S >> CM2_X_RP1_M2 & _S >> CM1_X_RP1_M1 & _S >> ROWS_P2;
    m |= _S << ROWS_P2 & _S << CM1_X_RP1_M1 & _S << CM2_X_RP1_M2;
    m |= _S >> ROWS_P2 & _S << CM2_X_RP1_M2 & _S << CM3_X_RP1_M3;
    m |= _S >> CM3_X_RP1_M3 & _S << ROWS_P2_X2 & _S << ROWS_P2;
    m |= _S >> CM2_X_RP1_M2 & _S << ROWS_P2 & _S >> ROWS_P2;
    m |= _S >> CM1_X_RP1_M1 & _S >> ROWS_P2 & _S >> ROWS_P2_X2;
    m |= _S << CM1_X_RP1_M1 & _S << CM2_X_RP1_M2 & _S << CM3_X_RP1_M3;

    return m | Connect4_original_fourInARow_threats(_S);
}

///////////////////////////////////////////////////
/// @brief  Can the player win on their turn?
/// @param  _C4
/// @return `true` if they can; otherwise `false`.
///////////////////////////////////////////////////
static inline bool Connect4_original_canWin(const Connect4 *const restrict _C4)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK);

    const Board SIDE = _C4->side;

    do // Guarantees at least one empty column
    {
        if (Connect4_fourInARow(SIDE | (dropMask & -dropMask)))
        {
            return true;
        }
    }
    while (dropMask &= dropMask - 1);

    return false;
}

////////////////////////////////////////////////////
/// @brief  Does the player lose on their turn?
/// @param  _C4
/// @return `true` if they lose; otherwise `false`.
////////////////////////////////////////////////////
static inline bool Connect4_misere_allLose(const Connect4 *const restrict _C4)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK);

    const Board SIDE = _C4->side;

    do
    {
        if (!Connect4_fourInARow(SIDE | (dropMask & -dropMask)))
        {
            return false;
        }
    }
    while (dropMask &= dropMask - 1);

    return true;
}

//////////////////////////////////////////////
/// @brief  A win-in-one detector for PopOut.
/// @param  _C4
//////////////////////////////////////////////
static inline bool Connect4_popout_canWin(const Connect4 *const restrict _C4)
{
    if (Connect4_original_canWin(_C4))
    {
        return true;
    }

    const Board PL_SIDE = _C4->side;

    for (Board popMask = Connect4_popMask(PL_SIDE); popMask; popMask &= popMask - 1)
    {
#if FTW_C4_MAX_BITS > 64
        const Board PL_FALL_COL = (PL_SIDE & COL_MASK << Connect4_ctz(popMask & -popMask)) >> 1 & ALL_MASK;
#else
        const Board PL_FALL_COL = (PL_SIDE & COL_MASK << stdc_trailing_zeros_ull(popMask & -popMask)) >> 1 & ALL_MASK;
#endif
        if (Connect4_popout_fourInARow(PL_FALL_COL | (PL_SIDE & ~PL_FALL_COL)))
        {
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////
/// @brief  Will a pop move result in an immediate loss?
/// @param  _C4
/////////////////////////////////////////////////////////
static inline bool Connect4_popout_popLose(const Connect4 *const restrict _C4)
{
    const Board PL_SIDE = _C4->side;
    const Board OP_SIDE = PL_SIDE ^ _C4->mask;

    Board popMask = Connect4_popMask(PL_SIDE); uint8_t oppWins;

#if FTW_C4_MAX_BITS > 64
    const uint8_t POP_BITS = Connect4_popcnt(popMask);
#else
    const uint8_t POP_BITS = stdc_count_ones_ull(popMask);
#endif

    for (oppWins = 0; popMask; popMask &= popMask - 1)
    {
#if FTW_C4_MAX_BITS > 64
        const Board OP_FALL_COL = (OP_SIDE & COL_MASK << Connect4_ctz(popMask & -popMask)) >> 1 & ALL_MASK;
#else
        const Board OP_FALL_COL = (OP_SIDE & COL_MASK << stdc_trailing_zeros_ull(popMask & -popMask)) >> 1 & ALL_MASK;
#endif
        if (Connect4_popout_fourInARow(OP_FALL_COL | (OP_SIDE & ~OP_FALL_COL)))
        {
            oppWins++;
        }
    }

    return _C4->mask == ALL_MASK && oppWins == POP_BITS;
}

//////////////////////////////////////////////////////
/// @brief  Does the player need one more pop to win?
/// @param  _SIDE
/// @param  _TGT
/// @return `true` if yes; otherwise `false`.
//////////////////////////////////////////////////////
static inline bool Connect4_pop10_canWin(const Board _SIDE, const uint8_t _TGT)
{
    return (Connect4_popMask(_SIDE)) && _TGT == POP10_TGT_M1;
}

////////////////////////////////////////////
/// @brief  Is the board full of pieces?
/// @param  _C4
/// @return `true` if full; `false` if not.
/// @note   Also acts as a ply limiter.
////////////////////////////////////////////
static inline bool Connect4_full(const Connect4 *const restrict _C4)
{
    return _C4->plies == PLY_LENGTH;
}

///////////////////////////////////////////////
/// @brief  Is the Connect 4 game over?
/// @param  _C4
/// @return `true` if over; otherwise `false`.
///////////////////////////////////////////////
static inline bool Connect4_original_over(const Connect4 *const restrict _C4)
{
    return Connect4_fourInARow(_C4->side ^ _C4->mask) || Connect4_full(_C4);
}

//////////////////////////////////////////////////
/// @brief  Is the game of Connect 4 PopOut over?
//////////////////////////////////////////////////
static inline bool Connect4_popout_over(const Connect4 *const restrict _C4)
{
    return Connect4_fourInARow(_C4->side) || Connect4_fourInARow(_C4->side ^ _C4->mask);
}

//////////////////////////////////////////////////////
/// @brief  Did the player met the collection target?
/// @param  _P10
/// @return `true` if reached; otherwise `false`.
//////////////////////////////////////////////////////
static inline bool Connect4_pop10_over(const Connect4_Pop10 *const restrict _P10)
{
    return (_P10->turn ? _P10->pops >> 4 : _P10->pops & 0xf) == POP10_TGT;
}

///////////////////////////////////////////////////
/// @brief  Counts the number of non-full columns.
///////////////////////////////////////////////////
static inline uint8_t Connect4_nonFullCols(const Connect4 *const restrict _C4)
{
#if FTW_C4_MAX_BITS > 64
    return COLS - Connect4_popcnt(Connect4_dropMask(_C4->mask, BOT_MASK, FULL_MASK));
#else
    return COLS - stdc_count_ones_ull(Connect4_dropMask(_C4->mask, BOT_MASK, FULL_MASK));
#endif
}

///////////////////////////////////////////////////////////////////////////////
/// @brief  Can players increase their collection count given the board state?
/// @return `true` if yes (effectively a draw); otherwise `false`.
///////////////////////////////////////////////////////////////////////////////
static inline bool Connect4_pop10_draw(const Connect4 *const restrict _C4, const Connect4_Pop10 *const restrict _P10)
{
    const Board PL_SIDE = _C4->side;
    const Board OP_SIDE = PL_SIDE ^ _C4->mask;

#if FTW_C4_MAX_BITS > 64
    const uint8_t PL_BOARD_COUNT = Connect4_popcnt(PL_SIDE) - 3;
    const uint8_t OP_BOARD_COUNT = Connect4_popcnt(OP_SIDE) - 3;
#else
    const uint8_t PL_BOARD_COUNT = stdc_count_ones_ull(PL_SIDE) - 3;
    const uint8_t OP_BOARD_COUNT = stdc_count_ones_ull(OP_SIDE) - 3;
#endif

    const uint8_t PL_PIECE_COUNT = _P10->turn ? _P10->pops >> 4 : _P10->pops & 0xf;
    const uint8_t OP_PIECE_COUNT = _P10->turn ? _P10->pops & 0xf : _P10->pops >> 4;

    return PL_PIECE_COUNT + PL_BOARD_COUNT <= POP10_TGT_M1 && OP_PIECE_COUNT + OP_BOARD_COUNT <= POP10_TGT_M1;
}

/////////////////////////////////////////////////////////////
/// @brief          Loop body for Connect 4 move generation.
/// @param  _mask   Bit mask of non-full/playable columns.
/// @param  _arr    Output array to store the moves.
/// @param  _num    Output variable to save this count.
/////////////////////////////////////////////////////////////
static inline void Connect4_original_genMoveBody(Board _mask, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    while (_mask)
    {
        const Board DISK_BIT = _mask & -_mask;

#if FTW_C4_MAX_BITS > 64
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        _arr[(*_num)++] = libdivide_u64_branchfree_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1);
    #else
        _arr[(*_num)++] = libdivide_u64_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1);
    #endif
    #else
        _arr[(*_num)++] = Connect4_ctz(DISK_BIT) / ROWS_P1;
    #endif
#else
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        _arr[(*_num)++] =  libdivide_u64_branchfree_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1);
    #else
        _arr[(*_num)++] = libdivide_u64_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1);
    #endif
    #else
        _arr[(*_num)++] = stdc_trailing_zeros_ull(DISK_BIT) / ROWS_P1;
    #endif
#endif

        _mask ^= DISK_BIT;
    }
}

/////////////////////////////////////////////////////////////////
/// @brief  PopOut version of `Connect4_original_genMoveBody()`.
/// @param  _mask
/// @param  _arr
/// @param  _num
/////////////////////////////////////////////////////////////////
static inline void Connect4_popout_genMoveBody(Board _mask, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    while (_mask)
    {
        const Board DISK_BIT = _mask & -_mask;

#if FTW_C4_MAX_BITS > 64
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        _arr[(*_num)++] = libdivide_u64_branchfree_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1) + COLS;
    #else
        _arr[(*_num)++] = libdivide_u64_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1) + COLS;
    #endif
    #else
        _arr[(*_num)++] = Connect4_ctz(DISK_BIT) / ROWS_P1 + COLS;
    #endif
#else
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        _arr[(*_num)++] = libdivide_u64_branchfree_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1) + COLS;
    #else
        _arr[(*_num)++] = libdivide_u64_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1) + COLS;
    #endif
    #else
        _arr[(*_num)++] = stdc_trailing_zeros_ull(DISK_BIT) / ROWS_P1 + COLS;
    #endif
#endif

        _mask ^= DISK_BIT;
    }
}

//////////////////////////////////////////////////////////////
/// @brief  Same as above, but performs column move ordering.
/// @param  _mask
/// @param  _arr
/// @param  _num
//////////////////////////////////////////////////////////////
static inline void Connect4_original_genMoveBodyOrder(Board _mask, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    while (_mask)
    {
        const Board DISK_BIT = _mask & -_mask;

#if FTW_C4_MAX_BITS > 64
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        Connect4_append(_arr, _num, libdivide_u64_branchfree_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1), 0, 0);
    #else
        Connect4_append(_arr, _num, libdivide_u64_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1), 0, 0);
    #endif
    #else
        Connect4_append(_arr, _num, Connect4_ctz(DISK_BIT) / ROWS_P1, 0, 0);
    #endif
#else
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        Connect4_append(_arr, _num, libdivide_u64_branchfree_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1), 0, 0);
    #else
        Connect4_append(_arr, _num, libdivide_u64_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1), 0, 0);
    #endif
    #else
        Connect4_append(_arr, _num, stdc_trailing_zeros_ull(DISK_BIT) / ROWS_P1, 0, 0);
    #endif
#endif

        _mask ^= DISK_BIT;
    }
}

//////////////////////////////////////////////////////////////////////
/// @brief  PopOut version of `Connect4_original_genMoveBodyOrder()`.
/// @param  _mask
/// @param  _arr
/// @param  _num
//////////////////////////////////////////////////////////////////////
static inline void Connect4_popout_genMoveBodyOrder(Board _mask, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    const uint8_t NUM = *_num;

    while (_mask)
    {
        const Board DISK_BIT = _mask & -_mask;

#if FTW_C4_MAX_BITS > 64
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        Connect4_append(_arr, _num, libdivide_u64_branchfree_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1) + COLS, NUM, COLS);
    #else
        Connect4_append(_arr, _num, libdivide_u64_do(Connect4_ctz(DISK_BIT), &libdivide_ROWS_P1) + COLS, NUM, COLS);
    #endif
    #else
        Connect4_append(_arr, _num, Connect4_ctz(DISK_BIT) / ROWS_P1 + COLS, NUM, COLS);
    #endif
#else
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        Connect4_append(_arr, _num, libdivide_u64_branchfree_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1) + COLS, NUM, COLS);
    #else
        Connect4_append(_arr, _num, libdivide_u64_do(stdc_trailing_zeros_ull(DISK_BIT), &libdivide_ROWS_P1) + COLS, NUM, COLS);
    #endif
    #else
        Connect4_append(_arr, _num, stdc_trailing_zeros_ull(DISK_BIT) / ROWS_P1 + COLS, NUM, COLS);
    #endif
#endif

        _mask ^= DISK_BIT;
    }
}

/////////////////////////////////////////////////////////////////////
/// @brief  Pop 10 adaption of `Connect4_popout_genMoveBodyOrder()`.
/// @param  _SIDE
/// @param  _mask
/// @param  _arr
/// @param  _num
/////////////////////////////////////////////////////////////////////
static inline void Connect4_pop10_genMoveBodyOrder(const Board _SIDE, Board _mask, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    uint8_t window = 0;

    while (_mask)
    {
        const Board DISK_BIT = _mask & -_mask;

#if FTW_C4_MAX_BITS > 64
        const Board DISK_OFFSET = Connect4_ctz(DISK_BIT);
#else
        const Board DISK_OFFSET = stdc_trailing_zeros_ull(DISK_BIT);
#endif

        const bool DISK_4ROW = Connect4_pop10_fourInARow(_SIDE, BOARD(1) << DISK_OFFSET);

#if FTW_C4_MAX_BITS > 64
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        Connect4_pop10_append(_arr, _num, libdivide_u64_branchfree_do(DISK_OFFSET, &libdivide_ROWS_P1) + COLS, 0, window, DISK_4ROW);
    #else
        Connect4_pop10_append(_arr, _num, libdivide_u64_do(CDISK_OFFSET, &libdivide_ROWS_P1) + COLS, 0, window, DISK_4ROW);
    #endif
    #else
        Connect4_pop10_append(_arr, _num, DISK_OFFSET / ROWS_P1 + COLS, 0, window, DISK_4ROW);
    #endif
#else
    #ifdef FTW_LIBDIVIDE
    #ifdef FTW_BRANCHLESS
        Connect4_pop10_append(_arr, _num, libdivide_u64_branchfree_do(DISK_OFFSET, &libdivide_ROWS_P1) + COLS, 0, window, DISK_4ROW);
    #else
        Connect4_pop10_append(_arr, _num, libdivide_u64_do(DISK_OFFSET, &libdivide_ROWS_P1) + COLS, 0, window, DISK_4ROW);
    #endif
    #else
        Connect4_pop10_append(_arr, _num, DISK_OFFSET / ROWS_P1 + COLS, 0, window, DISK_4ROW);
    #endif
#endif

        window += DISK_4ROW;
        _mask ^= DISK_BIT;
    }
}

///////////////////////////////////////////////////////////////////
/// @brief  Default Connect 4 move generator that filters threats.
/// @param  _C4
/// @param  _arr
/// @param  _num
///////////////////////////////////////////////////////////////////
static inline void Connect4_original_generate(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK); *_num = 0;

    const Board PL_THREATS = Connect4_fourInARow_threats(_C4->side) & dropMask;
    const Board OP_THREATS = Connect4_fourInARow_threats(_C4->side ^ _C4->mask) & dropMask;

    dropMask = PL_THREATS ? PL_THREATS : (OP_THREATS ? OP_THREATS : dropMask);

    Connect4_original_genMoveBody(dropMask, _arr, _num);
}

////////////////////////////////////////////////////////////////
/// @brief  Version of the move generator for Misere Connect 4.
/// @param  _C4
/// @param  _arr
/// @param  _num
////////////////////////////////////////////////////////////////
static inline void Connect4_misere_generate(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK); *_num = 0;

    const Board PL_THREATS = Connect4_fourInARow_threats(_C4->side) & dropMask;

    dropMask = PL_THREATS && PL_THREATS != dropMask ? dropMask ^ PL_THREATS : dropMask;

    Connect4_original_genMoveBody(dropMask, _arr, _num);
}

//////////////////////////////////////////////////////////
/// @brief  Compiles a list of moves in Connect 4 PopOut.
//////////////////////////////////////////////////////////
static inline void Connect4_popout_generate(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Connect4_original_generate(_C4, _arr, _num);
    Connect4_popout_genMoveBody(Connect4_popMask(_C4->side), _arr, _num);
}

///////////////////////////////////////
/// @brief  The Pop 10 move generator.
/// @param  _C4
/// @param  _p10
/// @param  _arr
/// @param  _num
///////////////////////////////////////
static inline void Connect4_pop10_generate(const Connect4 *const restrict _C4, Connect4_Pop10 *const restrict _p10, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    *_num = 0;

    if (_p10->phase)
    {
        Connect4_pop10_passForced(_C4, _p10) ? _arr[(*_num)++] = POP10_PASS : Connect4_pop10_genMoveBodyOrder(_C4->side, Connect4_popMask(_C4->side), _arr, _num);
    }
    else
    {
        Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK);

        Connect4_nonFullCols(_C4) > 1 ? dropMask &= ~(COL_MASK << (ROWS_P1 * (_C4->hist[_C4->plies - 1] - COLS))) : FTW_VOID_NOP;
        Connect4_original_genMoveBody(dropMask, _arr, _num);
    }
}

////////////////////////////////////////////////////////////////////////////////////
/// @brief  Generates every possible move in Connect 4, including non-winning ones.
/// @param  _C4
/// @param  _arr
/// @param  _num
////////////////////////////////////////////////////////////////////////////////////
static inline void Connect4_original_generateAll(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK); *_num = 0;

    Connect4_original_genMoveBody(dropMask, _arr, _num);
}

///////////////////////////////////////////////////////////////////
/// @brief  The analogous all-moves function for Connect 4 PopOut.
/// @param  _C4
/// @param  _arr
/// @param  _num
///////////////////////////////////////////////////////////////////
static inline void Connect4_popout_generateAll(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Connect4_original_generateAll(_C4, _arr, _num);
    Connect4_popout_genMoveBody(Connect4_popMask(_C4->side), _arr, _num);
}

//////////////////////////////////////////////////////////////////////
/// @brief      Non-losing version of `Connect4_original_generate()`.
/// @details    Reduces node evaluations for minimax search.
//////////////////////////////////////////////////////////////////////
static inline void Connect4_original_genNonLosing(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK); *_num = 0;
    Board opThreats = Connect4_fourInARow_threats(_C4->side ^ _C4->mask) & dropMask;

#if FTW_C4_MAX_BITS > 64 // cannot avoid losing; limit to 1 branch
    Connect4_popcnt(opThreats) > 1 ? (opThreats &= opThreats - 1) : FTW_VOID_NOP;
#else
    stdc_count_ones_ull(opThreats) > 1 ? (opThreats &= opThreats - 1) : FTW_VOID_NOP;
#endif

    dropMask = opThreats ? opThreats : dropMask;

    Connect4_original_genMoveBodyOrder(dropMask, _arr, _num);
}

//////////////////////////////////////////////////////////////////
/// @brief  Misere version of `Connect4_original_genNonLosing()`.
/// @param  _C4
/// @param  _arr
/// @param  _num
//////////////////////////////////////////////////////////////////
static inline void Connect4_misere_genNonLosing(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Board dropMask = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK); *_num = 0;

    const Board PL_THREATS = Connect4_fourInARow_threats(_C4->side) & dropMask;

    dropMask = PL_THREATS ? dropMask ^ PL_THREATS : dropMask;

    Connect4_original_genMoveBodyOrder(dropMask, _arr, _num);
}

//////////////////////////////////////////////////////////
/// @brief  The non-losing detector for Connect 4 PopOut.
/// @param  _C4
/// @param  _arr
/// @param  _num
//////////////////////////////////////////////////////////
static inline void Connect4_popout_genNonLosing(const Connect4 *const restrict _C4, uint8_t _arr[restrict static 4], uint8_t *const restrict _num)
{
    Connect4_original_genNonLosing(_C4, _arr, _num);
    Connect4_popout_genMoveBodyOrder(Connect4_popMask(_C4->side), _arr, _num);
}

/////////////////////////////////////////////////////
/// @brief  A dummy or no move policy for Connect 4.
/////////////////////////////////////////////////////
static inline char Connect4_noMovePolicy(const Connect4 *const restrict _C4)
{
    (void)(_C4);

    return '\0';
}

/////////////////////////////////////////////////////////
/// @brief  Zero-ply move policy for original Connect 4.
/// @param  _C4
/// @return 1-indexed column number; otherwise NULL.
/////////////////////////////////////////////////////////
static inline char Connect4_original_policy(const Connect4 *const restrict _C4)
{
    const Board DROP_MASK = Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK);
    const Board PL_THREATS = Connect4_fourInARow_threats(_C4->side) & DROP_MASK;
    const Board OP_THREATS = Connect4_fourInARow_threats(_C4->side ^ _C4->mask) & DROP_MASK;

    uint8_t policies[MOVE_BOUNDS];
    uint8_t polyCnt = 0;

    Connect4_original_genMoveBody(PL_THREATS, policies, &polyCnt);

    if (PL_THREATS)
    {
        goto Connect4_original_policy_playable;
    }

    Connect4_original_genMoveBody(OP_THREATS, policies, &polyCnt);

    if (OP_THREATS)
    {
        goto Connect4_original_policy_playable;
    }

    return Connect4_noMovePolicy(_C4);

Connect4_original_policy_playable:
    return policies[Xoshiro128pp_nextN(&g_rng, polyCnt)] + '1';
}

////////////////////////////////////////////////////////
/// @brief  PopOut version of the zero-ply move policy.
/// @param  _C4
////////////////////////////////////////////////////////
static inline char Connect4_popout_policy(const Connect4 *const restrict _C4)
{
    uint8_t policies[MOVE_BOUNDS];
    uint8_t polyCnt = 0;

    Connect4_original_genMoveBody(Connect4_fourInARow_threats(_C4->side) & Connect4_dropMask(_C4->mask, BOT_MASK, ALL_MASK), policies, &polyCnt);

    for (Board sideBotMask = Connect4_popMask(_C4->side); sideBotMask; sideBotMask &= sideBotMask - 1)
    {
#if FTW_C4_MAX_BITS > 64
        const Board FALL_COL = (_C4->side & COL_MASK << Connect4_ctz(sideBotMask & -sideBotMask)) >> 1 & ALL_MASK;
#else
        const Board FALL_COL = (_C4->side & COL_MASK << stdc_trailing_zeros_ull(sideBotMask & -sideBotMask)) >> 1 & ALL_MASK;
#endif

        if (Connect4_popout_fourInARow(FALL_COL | (_C4->side & ~FALL_COL)))
        {
#if FTW_C4_MAX_BITS > 64
    #ifdef FTW_LIBDIVIDE
        #ifdef FTW_BRANCHLESS
            policies[polyCnt++] = libdivide_u64_branchfree_do(Connect4_ctz(sideBotMask), &libdivide_ROWS_P1) + COLS;;
        #else
            policies[polyCnt++] = libdivide_u64_do(Connect4_ctz(sideBotMask), &libdivide_ROWS_P1) + COLS;
        #endif
    #else
            policies[polyCnt++] = Connect4_ctz(sideBotMask) / ROWS_P1 + COLS;
    #endif
#else
    #ifdef FTW_LIBDIVIDE
        #ifdef FTW_BRANCHLESS
            policies[polyCnt++] =  libdivide_u64_branchfree_do(stdc_trailing_zeros_ull(sideBotMask), &libdivide_ROWS_P1) + COLS;;
        #else
            policies[polyCnt++] = libdivide_u64_do(stdc_trailing_zeros_ull(sideBotMask), &libdivide_ROWS_P1) + COLS;
        #endif
    #else
            policies[polyCnt++] = stdc_trailing_zeros_ull(sideBotMask) / ROWS_P1 + COLS;
    #endif
#endif
        }
    }

    if (polyCnt)
    {
        const char POLICY = policies[Xoshiro128pp_nextN(&g_rng, polyCnt)];

        return POLICY < COLS ? POLICY + '1' : POLICY + 'A' - COLS;
    }

    return Connect4_noMovePolicy(_C4);
}

//////////////////////////////////////////////////
/// @brief  Parses a character and applies moves.
/// @param  _c4
/// @param  _CHR
/// @return `true` if valid; otherwise `false`.
//////////////////////////////////////////////////
static inline bool Connect4_original_parse(Connect4 *const restrict _c4, const char _CHR)
{
    int8_t token = _CHR - '1';

    for (uint8_t i = 0; i < 2; i++)
    {
        if (token < 0)
        {
            token = 10; // '0'
        }

        if (token >= 0 && token < COLS && !Connect4_over(_c4) && Connect4_droppable(_c4, token))
        {
            Connect4_play(_c4, token);

            return true;
        }

        token = toupper(_CHR) + 'A' - '1';
    }

    return false;
}

////////////////////////////////////////////////////////////////////
/// @brief  Makes a pop move after parsing an alphabetic character.
/// @param  _c4
/// @param  _CHR
/// @return `true` if valid; otherwise `false`.
////////////////////////////////////////////////////////////////////
static inline bool Connect4_popout_parse(Connect4 *const restrict _c4, const char _CHR)
{
    if (Connect4_original_parse(_c4, _CHR))
    {
        return true;
    }

    int8_t token = toupper(_CHR) - 'A';

    if (token >= 0 && token < COLS && !Connect4_over(_c4) && Connect4_poppable(_c4, token))
    {
        Connect4_pop(_c4, token);

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
/// @brief  Parses user input and applies moves in the Pop 10 varant.
/// @param _c4
/// @param _p10
/// @param _CHR
/// @return `true` if valid; otherwise `false`.
//////////////////////////////////////////////////////////////////////
static inline bool Connect4_pop10_parse(Connect4 *const restrict _c4, Connect4_Pop10 *const restrict _p10, const char _CHR)
{
    int8_t token;

    if (_p10->phase)
    {
        token = toupper(_CHR) - 'A';

        if (token >= 0 && token < COLS && !Connect4_pop10_over(_p10) && Connect4_poppable(_c4, token))
        {
            Connect4_pop10_pop(_c4, _p10, token);

            return true;
        }
    }
    else
    {
        token = _CHR - '1';

        const uint8_t NON_FULLS = Connect4_nonFullCols(_c4);
        const uint8_t HIST_COL = _c4->hist[_c4->plies - 1] - COLS;

        if (token >= 0 && token < COLS && (NON_FULLS == 1 || (NON_FULLS >= 2 && token != HIST_COL)) && Connect4_droppable(_c4, token))
        {
            Connect4_pop10_drop(_c4, _p10, token);

            return true;
        }
    }

    if (_CHR == POP10_PASS_CHR && Connect4_pop10_passForced(_c4, _p10))
    {
        Connect4_pop10_passMove(_c4, _p10);

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////
/// @brief  Announces the winner of Connect 4 to the player.
/// @param  _C4
/// @return `-1` => Ongoing; `0` => Draw; `1` => P1; `2` => P2.
////////////////////////////////////////////////////////////////
static inline int Connect4_original_winner(const Connect4 *const restrict _C4)
{
    const bool PL = _C4->plies & 1;

    if (Connect4_fourInARow(_C4->side ^ _C4->mask))
    {
        return !PL ? 2 : 1;
    }

    if (Connect4_fourInARow(_C4->side))
    {
        return PL ? 2 : 1;
    }

    return -!Connect4_full(_C4);
}

//////////////////////////////////////////////////////////
/// @brief  The opposite of `Connect4_original_winner()`.
/// @param  _C4
//////////////////////////////////////////////////////////
static inline int Connect4_misere_winner(const Connect4 *const restrict _C4)
{
    const int C4_WINNER = Connect4_original_winner(_C4);

    return C4_WINNER == 1 ? 2 : C4_WINNER == 2 ? 1 : C4_WINNER;
}

//////////////////////////////////////////////////////////////
/// @brief      Announces the winner of Pop 10 to the player.
/// @param      _P10
/// @return     `-1` => Ongoing; `1` => P1; `2` => P2.
/// @attention   Draws or ties are not well-defined.
//////////////////////////////////////////////////////////////
static inline int Connect4_pop10_winner(const Connect4_Pop10 *const restrict _P10)
{
    if (Connect4_pop10_over(_P10))
    {
        return _P10->turn ? 2 : 1;
    }

    return -1;
}

//////////////////////////////////////////////////////////////////
/// @brief  Obtains a perfect (collision-free) key for Connect 4.
/// @param  _C4
//////////////////////////////////////////////////////////////////
static inline Board Connect4_key(const Connect4 *const restrict _C4)
{
    return _C4->mask + _C4->side;
}

#ifdef FTW_AI_DEBUG
/**
 * @brief Reconstruct the bitboards represented by a perfect Connect 4 key.
 *
 * Within one column of height `h`, `mask` is `(1 << h) - 1` and
 * `side` is a subset of that mask. Consequently, `mask + side` occupies the
 * disjoint interval `[2^h - 1, 2^(h + 1) - 2]`; this makes the column height
 * and both bitboards recoverable without move history.
 *
 * @param _c4 State receiving the decoded bitboards. Its existing `hist`
 *            pointer is preserved, and `plies` is reset to zero.
 * @param _KEY Raw key returned by Connect4_key().
 */
static inline void Connect4_fromKey(Connect4 *const restrict _c4,
                                    const Board _KEY)
{
    Board mask = 0, side = 0;

    for (uint8_t column = 0; column < COLS; column++)
    {
        const uint8_t OFFSET = ROWS_P1 * column;
        const Board COLUMN_KEY = _KEY >> OFFSET & ALL_COL_MASK;
        Board value = COLUMN_KEY + 1;
        uint8_t height = 0;

        while (value >>= 1)
        {
            height++;
        }

        assert(height <= ROWS);
        const Board COLUMN_MASK = (BOARD(1) << height) - 1;
        const Board COLUMN_SIDE = COLUMN_KEY - COLUMN_MASK;
        assert(!(COLUMN_SIDE & ~COLUMN_MASK));

        mask |= COLUMN_MASK << OFFSET;
        side |= COLUMN_SIDE << OFFSET;
    }

    _c4->mask = mask;
    _c4->side = side;
    _c4->plies = 0;
    assert(Connect4_key(_c4) == _KEY);
}
#endif

////////////////////////////////////////////////
/// @brief  Hash keys for Pop 10's state field.
/// @param  _P10
////////////////////////////////////////////////
static inline Board Connect4_pop10_stateKey(const Connect4_Pop10 *const restrict _P10)
{
    return _P10->turn | (Board)(_P10->phase) << 1 | (Board)(_P10->pops) << 2;
}

///////////////////////////////
/// @brief  Pop 10 key hasher.
/// @param  _C4
/// @param  _P10
///////////////////////////////
static inline Board Connect4_pop10_key(const Connect4_Pop10 *const restrict _P10)
{
    return Connect4_pop10_stateKey(_P10) << (BOARD_AREA + ROWS_P1);
}

//////////////////////////////////////////////////////////////
/// @brief  Connect 4 lock function for transposition tables.
/// @param  _C4
/// @note   Returns the perfect key unless board > 64 bits.
//////////////////////////////////////////////////////////////
static inline TTLock Connect4_lock(const Connect4 *const restrict _C4)
{
#ifdef FTW_TT_128_BITS
#if FTW_C4_MAX_BITS > 128
#ifdef FTW_XXHASH
    const XXH128_hash_t LOCK = XXH3_128bits_withSeed(_C4, C4_SIZE, C4_xxhSeed);
    return LOCK.low64 | (TTLock)(LOCK.high64) << 64;
#else // Shrink to 128 bits (MurmurHash3)
    const Murmur128 LOCK = Murmur3_x64_128(_C4, C4_SIZE, C4_SALT_A);
    return LOCK.h2 | (TTLock)(LOCK.h1) << 64;
#endif
#else
#ifdef FTW_XXHASH
    const XXH128_hash_t LOCK = XXH3_128bits_withSeed(_C4, C4_SIZE, C4_xxhSeed);
    return LOCK.low64 | (TTLock)(LOCK.high64) << 64;
#else
    return Connect4_key(_C4);
#endif
#endif
#else
#if FTW_C4_MAX_BITS > 64
#ifdef FTW_XXHASH
    return XXH3_64bits_withSeed(_C4, C4_SIZE, C4_xxhSeed);
#else // 128 bits -> 64 bits (SplitMix64)
    const Board B_KEY_RAW = Connect4_key(_C4);
    const TTLock B_KEY_HI64 = B_KEY_RAW >> 64;
    const TTLock B_KEY_LO64 = B_KEY_RAW;
    return SplitMix64_finalize((B_KEY_HI64 + C4_SALT_A) ^ SplitMix64_finalize(B_KEY_LO64 + C4_SALT_B));
#endif
#else
#ifdef FTW_XXHASH
    return XXH3_64bits_withSeed(_C4, C4_SIZE, C4_xxhSeed);
#else
    return Connect4_key(_C4);
#endif
#endif
#endif
}

///////////////////////////////////////////////////////////////
/// @brief  Horizontally reverses a Connect 4 bitboard or key.
/// @param  _B
///////////////////////////////////////////////////////////////
static inline Board Connect4_reverse(const Board _B)
{
    Board rev = 0;

    for (uint8_t i = 0;; i++)
    {
        const uint8_t L_HALF = ROWS_P1 * i;
        const uint8_t R_HALF = ROWS_P1 * (COLS_M1 - i);
        const int8_t R_M_L = R_HALF - L_HALF;

        if (R_M_L < 0)
        {
            return rev;
        }

        rev |= (_B & ALL_COL_MASK << L_HALF) << R_M_L;
        rev |= (_B & ALL_COL_MASK << R_HALF) >> R_M_L;
    }
}

/////////////////////////////////////////////////////////////////
/// @brief  Rotates a Connect 4 bitboard one column to the left.
/// @param  _B
/////////////////////////////////////////////////////////////////
static inline Board Connect4_rotateBoardLeft(const Board _B)
{
    return _B >> ROWS_P1 | (_B & ALL_COL_MASK) << ROWS_P1 * COLS_M1;
}

//////////////////////////////////////////////////////////////////
/// @brief  Rotates a Connect 4 bitboard one column to the right.
/// @param  _B
//////////////////////////////////////////////////////////////////
static inline Board Connect4_rotateBoardRight(const Board _B)
{
    const uint8_t EDGE_SHIFT = ROWS_P1 * COLS_M1;

    return (_B << ROWS_P1 | (_B & ALL_COL_MASK << EDGE_SHIFT) >> EDGE_SHIFT) & ALL_MASK;
}

////////////////////////////////////////////////////////////
/// @brief  Shifts the move history one column to the left.
/// @param  _c4
////////////////////////////////////////////////////////////
static inline void Connect4_rotateHistLeft(Connect4 *const restrict _c4)
{
    for (uint16_t i = 0; i < _c4->plies; i++)
    {
        _c4->hist[i] = (_c4->hist[i] + COLS_M1) % COLS;
    }
}

/////////////////////////////////////////////////////////////
/// @brief  Shifts the move history one column to the right.
/////////////////////////////////////////////////////////////
static inline void Connect4_rotateHistRight(Connect4 *const restrict _c4)
{
    for (uint16_t i = 0; i < _c4->plies; i++)
    {
        _c4->hist[i] = (_c4->hist[i] + 1) % COLS;
    }
}

//////////////////////////////////////////////////////////////////
/// @brief      Canonicalizes a Connect 4 key with the lower one.
/// @param      _key
/// @attention  Assumes horizontal symmetry about the center.
//////////////////////////////////////////////////////////////////
static inline Board Connect4_original_canonicalize(Board _key)
{
    const Board REV_KEY = Connect4_reverse(_key);

    return _key < REV_KEY ? _key : REV_KEY;
}

///////////////////////////////////////////////////////////////////
/// @brief  Cylindrical version of Connect 4 key canonicalization.
/// @param  _key
///////////////////////////////////////////////////////////////////
static inline Board Connect4_cylinder_canonicalize(Board _key)
{
    Board rotKey = Connect4_original_canonicalize(_key);

    for (uint8_t i = 0; i < 2; i++)
    {
        for (uint8_t j = 0; j < COLS; j++)
        {
            rotKey = Connect4_rotateBoardLeft(rotKey);
            _key = rotKey < _key ? rotKey : _key;
        }

        rotKey = Connect4_reverse(_key);
    }

    return _key;
}

///////////////////////////////////////////////////////////////////
/// @brief      Is the board's left and right halves identical?
/// @param _C4  Unaliased pointer to the Connect 4 game state.
/// @return     `true` if it is a mirror image; otherwise `false`.
///////////////////////////////////////////////////////////////////
static inline bool Connect4_symmetric(const Connect4 *const restrict _C4)
{
    const Board PL = _C4->side;
    const Board OP = PL ^ _C4->mask;

    for (uint8_t i = 0; i < MID_COL; i++)
    {
        const uint8_t L_COL_BIT = ROWS_P1 * i;
        const uint8_t R_COL_BIT = ROWS_P1 * (COLS_M1 - i);

        const Board PL_COL_LEFT = (PL & COL_MASK << L_COL_BIT) >> L_COL_BIT;
        const Board OP_COL_LEFT = (OP & COL_MASK << L_COL_BIT) >> L_COL_BIT;

        const Board PL_COL_RIGHT = PL >> R_COL_BIT & COL_MASK;
        const Board OP_COL_RIGHT = OP >> R_COL_BIT & COL_MASK;

        if (PL_COL_LEFT != PL_COL_RIGHT || OP_COL_LEFT != OP_COL_RIGHT)
        {
            return false;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
/// @brief  Static column move order for unknown Connect 4 variants.
/// @param  _A
/////////////////////////////////////////////////////////////////////
static inline uint8_t Connect4_unknown_sequence(const uint8_t _A)
{
    return _A;
}

/////////////////////////////////////////////////////////////
/// @brief  Static column move order for Original Connect 4.
/// @param  _A
/////////////////////////////////////////////////////////////
static inline uint8_t Connect4_original_sequence(const uint8_t _A)
{
    return MID_COL + (1 - 2 * (_A & 1)) * ((_A + 1) >> 1);
}

///////////////////////////////////////////////////////////
/// @brief  Static column move order for Misere Connect 4.
/// @param  _A
///////////////////////////////////////////////////////////
static inline uint8_t Connect4_misere_sequence(const uint8_t _A)
{
    return (1 - (_A & 1)) * (_A >> 1) + (_A & 1) * (COLS_M1 - (_A - 1) / 2);
}

///////////////////////////////////////////////////////////////
/// @brief  Sets the function pointers for Connect 4 variants.
///////////////////////////////////////////////////////////////
static inline void Connect4_funcPtrs_init(void)
{
    Connect4_play = Connect4_drop;
    Connect4_unplay = Connect4_undrop;
    Connect4_playable = Connect4_droppable;
    Connect4_moves = Connect4_original_moves;
    Connect4_sequence = Connect4_original_sequence;
    Connect4_generate = Connect4_original_generate;
    Connect4_generateAll = Connect4_original_generateAll;
    Connect4_genNonLosing = Connect4_original_genNonLosing;
    Connect4_fourInARow = Connect4_original_fourInARow;
    Connect4_fourInARow_threats = Connect4_original_fourInARow_threats;
    Connect4_over = Connect4_original_over;
    Connect4_canWin = Connect4_original_canWin;
    Connect4_policy = Connect4_original_policy;
    Connect4_winner = Connect4_original_winner;
    Connect4_canonicalize = Connect4_original_canonicalize;
    Connect4_parse = Connect4_original_parse;

    switch (C4_variant) // Variant-specific overrides
    {
    case CONNECT4_MISERE:
        Connect4_sequence = Connect4_misere_sequence;
        Connect4_generate = Connect4_misere_generate;
        Connect4_genNonLosing = Connect4_misere_genNonLosing;
        Connect4_canWin = Connect4_misere_allLose;
        Connect4_policy = Connect4_noMovePolicy;
        Connect4_winner = Connect4_misere_winner;
        break;
    case CONNECT4_CYLINDER:
        Connect4_sequence = Connect4_unknown_sequence;
        Connect4_fourInARow = Connect4_cylinder_fourInARow;
        Connect4_fourInARow_threats = Connect4_cylinder_fourInARow_threats;
        Connect4_canonicalize = Connect4_cylinder_canonicalize;
        break;
    case CONNECT4_POPOUT:
        Connect4_play = Connect4_popout_play;
        Connect4_unplay = Connect4_popout_unplay;
        Connect4_playable = Connect4_popout_playable;
        Connect4_moves = Connect4_popout_moves;
        Connect4_generate = Connect4_popout_generate;
        Connect4_generateAll = Connect4_popout_generateAll;
        Connect4_genNonLosing = Connect4_popout_genNonLosing;
        Connect4_over = Connect4_popout_over;
        Connect4_canWin = Connect4_popout_canWin;
        Connect4_policy = Connect4_popout_policy;
        Connect4_parse = Connect4_popout_parse;
        break;
    case CONNECT4_POP10:
        Connect4_moves = Connect4_popout_moves;
        Connect4_policy = Connect4_noMovePolicy;
        Connect4_sequence = Connect4_unknown_sequence;
    default:
        break;
    }
}

#endif // CONNECT4_H //
