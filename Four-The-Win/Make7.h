/*
 *  Author: 2026- TheTrustedComputer
 *
 *  This is a computer implementation of Make 7, a niche Connect 4 variant involving elementary addition.
 *  The objective is to arrange numbers that add up to seven in a vertical, horizontal, or diagonal matter.
 *
 *  The physical game ships 11 one tiles, 11 two tiles, and 4 three tiles per player, totaling 52 tiles.
 *  This structure enforces them so that when any player runs out of tiles, they may not drop any more.
 *
 *  Below is a visual illustration on how the playing grid is internally represented in the data structure.
 *
 *  Playing grid            Player 1        Player 2       No. 1 tiles    No. 2 tiles    No. 3 tiles*
 *   .  .  .  .  .  .  .    0 0 0 0 0 0 0   0 0 0 0 0 0 0  0 0 0 0 0 0 0  0 0 0 0 0 0 0  0 0 0 0 0 0 0
 *   .  .  *  .  *  .  .    0 0 0 0 0 0 0   0 0 0 0 0 0 0  0 0 0 0 0 0 0  0 0 0 0 0 0 0  0 0 0 0 0 0 0
 *   .  *  . (2) .  *  .    0 0 0 0 0 0 0   0 0 0 1 0 0 0  0 0 0 0 0 0 0  0 0 0 1 0 0 0  0 0 0 0 0 0 0
 *   .  .  . (3) .  .  .    0 0 0 0 0 0 0   0 0 0 1 0 0 0  0 0 0 0 0 0 0  0 0 0 0 0 0 0  0 0 0 1 0 0 0
 *   *  .  . (1)[1] . (3)   0 0 0 0 1 0 0   0 0 0 1 0 0 1  0 0 0 1 1 0 0  0 0 0 0 0 0 0  0 0 0 0 0 0 1
 *   .  . [2](1)[2] . [1]   0 0 1 0 1 0 1   0 0 0 1 0 0 0  0 0 0 1 0 0 1  0 0 1 0 1 0 0  0 0 0 0 0 0 0
 *   . (2)[1][2][1](1)[2]   0 0 1 1 1 0 1   0 1 0 0 0 1 0  0 0 1 0 1 1 0  0 1 0 1 0 0 1  0 0 0 0 0 0 0
 *
 *  Sym   Description       * Storing #3 tiles are not required and can be computed with this statement:
 *  [1] | Player 1's tiles  * Three_Tiles = (Player1_Tiles | Player2_Tiles) ^ (One_Tiles | Two_Tiles);
 *  (1) | Player 2's tiles
 *   .  | Empty (1, 2)
 *   *  | Empty (1, 2, 3)
 *
 *  As of this writing, there are no other Make 7 programs besides ours.
 */

#ifndef MAKE7_H
#define MAKE7_H

// Constants
static constexpr uint8_t MAKE7_SIZE = 7;
static constexpr uint8_t MAKE7_SIZE_M1 = 6;
static constexpr uint8_t MAKE7_SIZE_P1 = 8;
static constexpr uint8_t MAKE7_SIZE_P2 = 9;
static constexpr uint8_t MAKE7_SIZE_X3 = 21;
static constexpr uint8_t MAKE7_SIZE_D2 = 3;
static constexpr uint8_t MAKE7_AREA = 49;
static constexpr uint8_t MAKE7_AREA_P1 = 50;
static constexpr uint8_t MAKE7_AREA_X2 = 98;
static constexpr uint8_t MAKE7_TILES_WIDTH = 11;
static constexpr uint8_t MAKE7_OFF1_MASK = 15;
static constexpr uint8_t MAKE7_OFF2_MASK = 15;
static constexpr uint8_t MAKE7_OFF3_MASK = 7;
static constexpr uint8_t MAKE7_OFF1_SHIFT = 0;
static constexpr uint8_t MAKE7_OFF2_SHIFT = 4;
static constexpr uint8_t MAKE7_OFF3_SHIFT = 8;

// Static bitmasks
static constexpr uint64_t MAKE7_ALL = 0x7f7f7f7f7f7f7f;
static constexpr uint64_t MAKE7_BOT = 0x1010101010101;
static constexpr uint64_t MAKE7_FULL = 0x80808080808080;
static constexpr uint64_t MAKE7_TOP_ROW = 0x40404040404040;
static constexpr uint64_t MAKE7_COL_MASK = 0x7f;
static constexpr uint64_t MAKE7_ALL_COL_MASK = 0xff;
static constexpr uint64_t MAKE7_THREES_MASK = 0x4102008201004;
static constexpr uint32_t MAKE7_INIT_AVAILS = 0x25dcbb; // [P2_33322221111][P1_33322221111]

// Make 7 column move ordering strategy (different from Connect 4)
static constexpr uint8_t MAKE7_COL_ORDER[MAKE7_SIZE] = { 2, 4, 1, 5, 3, 0, 6 };
static constexpr uint8_t MAKE7_REV_ORDER[23] = { 0o5, 0o2, 0o0, 0o4, 0o1, 0o3, 0o6, 0xff, 0o15, 0o12, 0o10, 0o14, 0o11, 0o13, 0o16, 0xff, 0o25, 0o22, 0o20, 0o24, 0o21, 0o23, 0o26 };

// Direction table to check for adjacent tiles
static constexpr uint8_t MAKE7_DIR_TABLE[3] = { MAKE7_SIZE_P1, MAKE7_SIZE, MAKE7_SIZE_P2 };

// Array of shift and masks to access the off tiles
static constexpr uint8_t MAKE7_OFF_SHIFTS[3] = { MAKE7_OFF1_SHIFT, MAKE7_OFF2_SHIFT, MAKE7_OFF3_SHIFT };
static constexpr uint8_t MAKE7_OFF_MASKS[3] = { MAKE7_OFF1_MASK, MAKE7_OFF2_MASK, MAKE7_OFF3_MASK };

// Bitmask table of adjacent tiles
// 1 1 1
// 1 0 1
// 1 1 1
static constexpr uint64_t ADJ_BITMASK_TABLE[55] = { 0x302ull, 0x705ull, 0xe0aull, 0x1c14ull, 0x3828ull, 0x7050ull, 0x6020ull, 0x0ull,
                                                    0x30203ull, 0x70507ull, 0xe0a0eull, 0x1c141cull, 0x382838ull, 0x705070ull, 0x602060ull, 0x0ull,
                                                    0x3020300ull, 0x7050700ull, 0xe0a0e00ull, 0x1c141c00ull, 0x38283800ull, 0x70507000ull, 0x60206000ull, 0x0ull,
                                                    0x302030000ull, 0x705070000ull, 0xe0a0e0000ull, 0x1c141c0000ull, 0x3828380000ull, 0x7050700000ull, 0x6020600000ull, 0x0ull,
                                                    0x30203000000ull, 0x70507000000ull, 0xe0a0e000000ull, 0x1c141c000000ull, 0x382838000000ull, 0x705070000000ull, 0x602060000000ull, 0x0ull,
                                                    0x3020300000000ull, 0x7050700000000ull, 0xe0a0e00000000ull, 0x1c141c00000000ull, 0x38283800000000ull, 0x70507000000000ull, 0x60206000000000ull, 0x0ull,
                                                    0x302030000000000ull, 0x705070000000000ull, 0xe0a0e0000000000ull, 0x1c141c0000000000ull, 0x3828380000000000ull, 0x7050700000000000ull, 0x6020600000000000ull };

static uint64_t MAKE7_SM_SALT, MAKE7_T1_SALT, MAKE7_T2_SALT;
static bool M7_targetMethod, (*Make7_targetSum_choice)(const uint64_t, const uint64_t, const uint64_t, const uint64_t, const uint8_t);

#pragma pack(push, 1)

typedef struct
{
    uint64_t mask, side;    // Bitmap of player/occupied tiles
    uint64_t tile1, tile2;  // Bitmap of tiles of value 1 / 2 (3 is implicit)
    uint64_t lastCol;       // Last dropped tile column bit index
    uint32_t avails;        // Bitfield of available tiles (see below for layout)
    uint8_t turn;           // Player turn (0=P1; 1=P2)
}
Make7;

// Layout of Make7::avails
// Bits 0-10: P1 off tiles
// Bits 11-21: P2 off tiles

#pragma pack(pop)

/////////////////////////////////////////////////////////////
/// @brief          Obtains the off tile count for a player.
/// @param _AVALS   Availability bitfield.
/// @param _TURN    Current player turn.
/// @param _TILE    Tile value, minus 1.
/////////////////////////////////////////////////////////////
static inline uint8_t Make7_count(const uint32_t _AVALS, const bool _TURN, const uint8_t _TILE)
{
    return _AVALS >> (MAKE7_OFF_SHIFTS[_TILE] + _TURN * MAKE7_TILES_WIDTH) & MAKE7_OFF_MASKS[_TILE];
}

//////////////////////////////////////////////
/// @brief  Returns the number of moves made.
/// @param  _M7
//////////////////////////////////////////////
static inline uint8_t Make7_moves(const Make7 *const restrict _M7)
{
    return stdc_count_ones_ull(_M7->mask);
}

/////////////////////////////////////////////////////////
/// @brief  Determines if the grid is full of tiles.
/// @param  _M7
/////////////////////////////////////////////////////////
static inline bool Make7_full(const Make7 *const restrict _M7)
{
    return _M7->mask == MAKE7_ALL;
}

///////////////////////////////////////////////////////////
/// @brief          Decrements the off tiles for a player.
/// @param  _aval   Availability bitfield.
/// @param  _TURN   Current player turn.
/// @param  _TILE   Tile value, minus 1.
///////////////////////////////////////////////////////////
static inline void Make7_decrement(uint32_t *const restrict _aval, const bool _TURN, const uint8_t _TILE)
{
    *_aval -= 1u << (MAKE7_OFF_SHIFTS[_TILE] + _TURN * MAKE7_TILES_WIDTH);
}

////////////////////////////////////////////////////////////////////
/// @brief          Does the board allow a tile drop into a column?
/// @param  _M7     Unaliased pointer to the game state.
/// @param  _TILE   Tile value, minus 1.
/// @param  _COL    Which column to check.
/// @return        `true` if permitted; otherwise `false`.
////////////////////////////////////////////////////////////////////
static inline bool Make7_droppable(const Make7 *const restrict _M7, const uint8_t _TILE, const uint8_t _COL)
{
    return Make7_count(_M7->avails, _M7->turn, _TILE) && ((MAKE7_COL_MASK << _COL * MAKE7_SIZE_P1 & (_M7->mask + MAKE7_BOT)) & (_TILE < 2 ? MAKE7_ALL : MAKE7_THREES_MASK));
}

////////////////////////////////////////////////////////
/// @brief  Resets a Make 7 game to the starting state.
/// @param  _m7
////////////////////////////////////////////////////////
static inline void Make7_reset(Make7 *const restrict _m7)
{
    _m7->mask = _m7->side = 0;
    _m7->tile1 = _m7->tile2 = 0;
    _m7->avails = MAKE7_INIT_AVAILS;
    _m7->lastCol = _m7->turn = 0;
}

///////////////////////////////////////
/// @brief  Initializes a Make 7 game.
/// @param  _m7
///////////////////////////////////////
static inline void Make7_init(Make7 *const restrict _m7)
{
    MOVE_SPACE = sizeof(MAKE7_REV_ORDER) / sizeof(*MAKE7_REV_ORDER);

    Xoshiro256 xsr256; Xoshiro256_init(&xsr256);

    MAKE7_SM_SALT = Xoshiro256ss_next(&xsr256);
    MAKE7_T1_SALT = Xoshiro256ss_next(&xsr256);
    MAKE7_T2_SALT = Xoshiro256ss_next(&xsr256);

    Make7_reset(_m7);
}

//////////////////////////////////////////////////////////////////
/// @brief      Prints the number tile grid and their quantities.
/// @param  _M7 Unaliased pointer to the Make 7 state.
//////////////////////////////////////////////////////////////////
static inline void Make7_print(const Make7 *const restrict _M7)
{
    const uint64_t PLAYER = _M7->side;
    const uint64_t ONE_TILES = _M7->tile1;
    const uint64_t TWO_TILES = _M7->tile2;
    const uint64_t THREE_TILES = _M7->mask ^ (ONE_TILES | TWO_TILES);

    for (uint8_t c = MAKE7_SIZE; c--;)
    {
        for (uint8_t r = 0; r < MAKE7_SIZE; r++)
        {
            const uint64_t BIT_POS = (UINT64_C(1) << MAKE7_SIZE_P1 * r) * (UINT64_C(1) << c);

            if (BIT_POS & ONE_TILES)
            {
                if (BIT_POS & PLAYER)
                {
                    _M7->turn ? printf(FTW_M7_ANSI_YELLOW_1) : printf(FTW_M7_ANSI_GREEN_1);
                }
                else
                {
                    _M7->turn ? printf(FTW_M7_ANSI_GREEN_1) : printf(FTW_M7_ANSI_YELLOW_1);
                }
            }
            else if (BIT_POS & TWO_TILES)
            {
                if (BIT_POS & PLAYER)
                {
                    _M7->turn ? printf(FTW_M7_ANSI_YELLOW_2) : printf(FTW_M7_ANSI_GREEN_2);
                }
                else
                {
                    _M7->turn ? printf(FTW_M7_ANSI_GREEN_2) : printf(FTW_M7_ANSI_YELLOW_2);
                }
            }
            else if (BIT_POS & THREE_TILES)
            {
                if (BIT_POS & PLAYER)
                {
                    _M7->turn ? printf(FTW_M7_ANSI_YELLOW_3) : printf(FTW_M7_ANSI_GREEN_3);
                }
                else
                {
                    _M7->turn ? printf(FTW_M7_ANSI_GREEN_3) : printf(FTW_M7_ANSI_YELLOW_3);
                }
            }
            else
            {
                (BIT_POS & MAKE7_THREES_MASK) ? printf(FTW_M7_ANSI_THREES_BLANK) : printf(FTW_M7_ANSI_COMMON_BLANK);
            }
        }

        if (c)
        {
            putchar('\n');
        }
    }

    putchar('\n');
}

////////////////////////////////////////////////////////////////
/// @brief          Drops a number tile into a non-full column.
/// @param  _m7     Unaliased pointer to the Make 7 state.
/// @param  _TILE   What number tile type to drop, minus 1.
/// @param  _COL    Which column to drop the tile into.
////////////////////////////////////////////////////////////////
static inline void Make7_drop(Make7 *const restrict _m7, const uint8_t _TILE, const uint8_t _COL)
{
    const uint64_t DROPPER = MAKE7_COL_MASK << MAKE7_SIZE_P1 * _COL & (_m7->mask + MAKE7_BOT);

    !_TILE ? _m7->tile1 |= DROPPER : _TILE <= 1 ? _m7->tile2 |= DROPPER : FTW_VOID_NOP;
    _m7->side ^= _m7->mask;
    _m7->mask |= DROPPER;
    _m7->lastCol = DROPPER;

    Make7_decrement(&_m7->avails, _m7->turn, _TILE);

    _m7->turn ^= 1;
}

////////////////////////////////////////////////////////////
/// @brief      Adaption of `Connect4_append()` for Make 7.
/// @param _arr Move array.
/// @param _num Move count.
/// @param _MOV Move to add.
/// @param _POS Array position.
////////////////////////////////////////////////////////////
static inline void Make7_append(uint8_t _arr[restrict static 1], uint8_t *const restrict _num, const uint8_t _MOV, const uint8_t _POS)
{
    _arr[(*_num)++] = _MOV;

    for (uint8_t i = *_num - _POS; --i;)
    {
        uint8_t *const restrict curr = &_arr[i + _POS], *const restrict prev = curr - 1;

        if (MAKE7_REV_ORDER[*curr] < MAKE7_REV_ORDER[*prev])
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

//////////////////////////////////////////////////////////////////////
/// @brief  Generates a list of legal Make 7 moves in octal notation.
/// @param  _M7
/// @param  _arr
/// @param  _num
//////////////////////////////////////////////////////////////////////
static inline void Make7_generate(const Make7 *const restrict _M7, uint8_t _arr[restrict static 1], uint8_t *const restrict _num)
{
    const uint8_t OFF_TILES[3] = { Make7_count(_M7->avails, _M7->turn, 0), Make7_count(_M7->avails, _M7->turn, 1), Make7_count(_M7->avails, _M7->turn, 2) };
    const uint64_t MOVE_COL_MASK = (_M7->mask + MAKE7_BOT) & MAKE7_ALL;

    uint64_t tileColMask = MOVE_COL_MASK & MAKE7_THREES_MASK;
    uint8_t tilePosition = 0; *_num = 0;

#ifdef __clang__
    #pragma clang loop unroll(enable)
#elifdef __GNUC__
    #pragma GCC unroll 3
#endif
    for (uint8_t tileIndex = 3; tileIndex--;)
    {
        const uint8_t TILE_SHIFT = tileIndex << 3;

        while (tileColMask && OFF_TILES[tileIndex])
        {
            const uint64_t TILE_MASK = tileColMask & -tileColMask;

            Make7_append(_arr, _num, TILE_SHIFT | stdc_trailing_zeros_ull(TILE_MASK) >> 3, tilePosition);

            tileColMask ^= TILE_MASK;
        }

        tileColMask = MOVE_COL_MASK;
        tilePosition = *_num;
    }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief          Does the entire maximal run of tiles meet the target sum?
/// @param  _P_T1   Bitmap of player tiles of value 1.
/// @param  _P_T2   Bitmap of player tiles of value 2.
/// @param  _P_T3   Bitmap of player tiles of value 3.
/// @param  _T_RUN  Bitmask of running tiles.
/// @param  _DUMMY  Unused parameter (number of tiles in window).
/// @return        `true` if the target is met; otherwise `false`.
//////////////////////////////////////////////////////////////////////////////
static inline bool Make7_targetSum_exact(const uint64_t _P_T1, const uint64_t _P_T2, const uint64_t _P_T3, const uint64_t _T_RUN, const uint8_t _DUMMY)
{
    (void)(_DUMMY);

    return stdc_count_ones_ull(_P_T1 & _T_RUN) + 2 * stdc_count_ones_ull(_P_T2 & _T_RUN) + 3 * stdc_count_ones_ull(_P_T3 & _T_RUN) == 7;
}

//////////////////////////////////////////////////////////////////////////
/// @brief          Finds a sub-run of tiles that reaches the target sum.
/// @param  _P_T1   Bitmap of player tiles of value 1.
/// @param  _P_T2   Bitmap of player tiles of value 2.
/// @param  _DUMMY  Unused parameter (tiles of value 3).
/// @param  _T_RUN  Bitmask of running tiles.
/// @param  _S_DIR  Adjacent tiles shift direction.
/// @param  _CNT    Number of tiles in the current window.
/// @return        `true` if the target is met; otherwise `false`.
//////////////////////////////////////////////////////////////////////////
static inline bool Make7_targetSum_window(const uint64_t _P_T1, const uint64_t _P_T2, const uint64_t _DUMMY, const uint64_t _T_RUN, const uint8_t _S_DIR)
{
    (void)(_DUMMY);

    // We use a 16-bit integer to store binary-encoded adjacent tiles (2 bits per tile; 7 tiles max; 14 bits).
    // 00: empty
    // 01: #1 tiles
    // 10: #2 tiles
    // 11: #3 tiles
    uint64_t tileBit = _T_RUN & -_T_RUN;
    uint16_t adjacents = 0;

    do
    {
        adjacents = adjacents << 2 | (_P_T1 & tileBit ? 1 : (_P_T2 & tileBit ? 2 : 3));
    }
    while ((tileBit <<= _S_DIR) & _T_RUN);

    // There are 8 unique ways to add given numbers 1, 2, and 3:
    //
    // 1. 3+3+1 = 7
    // 2. 3+2+2 = 7
    // 3. 3+2+1+1 = 7
    // 4. 3+1+1+1+1 = 7
    // 5. 2+2+2+1 = 7
    // 6. 2+2+1+1+1 = 7
    // 7. 2+1+1+1+1+1 = 7
    // 8. 1+1+1+1+1+1+1 = 7
    //
    // Addition is commutative; the sum can be in any order. Thus, that figure jumps to 44 ways.
    // Below is the sliding window algorithm starting with a window size of 3 when the window sum exceeds 7.
    // This approach does not use auxiliary arrays; thus, reducing the overhead of a memory access.
    //
    // 0x3  = 00000000000011
    // 0xc  = 00000000001100
    // 0x30 = 00000000110000
    uint8_t winEnd = 4, winSum = (adjacents & 0x3) + ((adjacents & 0xc) >> 2) + ((adjacents & 0x30) >> winEnd);

    do
    {
        if (winSum == 7)
        {
            return true;
        }

        winSum < 7 ? (winEnd += 2, winSum += (adjacents & 0x3 << winEnd) >> winEnd) : (winEnd -= 2, winSum -= (adjacents & 0x3), adjacents >>= 2);
    }
    while (adjacents & 0x3 << winEnd);

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////
/// @brief  Checks whether a maximal run of player tiles matches the target sum of 7.
/// @param  _M7
/// @return `true` if yes; otherwise `false`.
//////////////////////////////////////////////////////////////////////////////////////
static inline bool Make7_targetSum(const Make7 *const restrict _M7)
{
    const uint64_t PLAYER_ALL_BITMASK = _M7->side ^ _M7->mask;
    const uint64_t PLAYER_ONES_BITMASK = PLAYER_ALL_BITMASK & _M7->tile1;
    const uint64_t PLAYER_TWOS_BITMASK = PLAYER_ALL_BITMASK & _M7->tile2;
    const uint64_t PLAYER_THREES_BITMASK = PLAYER_ALL_BITMASK ^ (PLAYER_ONES_BITMASK | PLAYER_TWOS_BITMASK);
    const uint64_t NONLAST_TILE_BITMASK = PLAYER_ALL_BITMASK ^ _M7->lastCol;
    const uint8_t LAST_DROP_TILE_INDEX = stdc_trailing_zeros_ull(_M7->lastCol);

    if ((LAST_DROP_TILE_INDEX != 64) && (NONLAST_TILE_BITMASK & ADJ_BITMASK_TABLE[LAST_DROP_TILE_INDEX]))
    {
        uint64_t tileMask = NONLAST_TILE_BITMASK, tileRun = _M7->lastCol; uint8_t tileCnt = 1;

        for (; tileMask & _M7->lastCol >> 1; tileMask &= tileMask << 1, tileCnt++, tileRun |= tileRun >> 1); // Vertical

        if (tileCnt >= 3 && Make7_targetSum_choice(PLAYER_ONES_BITMASK, PLAYER_TWOS_BITMASK, PLAYER_THREES_BITMASK, tileRun, 1))
        {
            return true;
        }

#ifdef __clang__
        #pragma clang loop unroll(enable)
#elifdef __GNUC__
        #pragma GCC unroll 3
#endif
        for (uint8_t dir = 0; dir < 3; dir++)
        {
            const uint8_t SHIFTER = MAKE7_DIR_TABLE[dir];

            tileRun = _M7->lastCol;
            tileCnt = 1;

            for (tileMask = NONLAST_TILE_BITMASK; tileMask & _M7->lastCol << SHIFTER; tileMask &= tileMask >> SHIFTER, tileCnt++, tileRun |= tileRun << SHIFTER);
            for (tileMask = NONLAST_TILE_BITMASK; tileMask & _M7->lastCol >> SHIFTER; tileMask &= tileMask << SHIFTER, tileCnt++, tileRun |= tileRun >> SHIFTER);

            if (tileCnt >= 3 && Make7_targetSum_choice(PLAYER_ONES_BITMASK, PLAYER_TWOS_BITMASK, PLAYER_THREES_BITMASK, tileRun, SHIFTER))
            {
                return true;
            }
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////
/// @brief      Selects a target sum (win condition) method.
/// @details    Exact: 2+2+3=7 => win; 2+2+2+3=9 => no win
///             Slider: 2+2+2+3 == 2+[2+2+3] => win
/////////////////////////////////////////////////////////////
static inline void Make7_setTargetMethod(void)
{
    Make7_targetSum_choice = M7_targetMethod ? Make7_targetSum_window : Make7_targetSum_exact;
}

//////////////////////////////////////////////////////
/// @brief  Sees if the player can win on their move.
/// @param  _M7
/// @return `true` if possible; otherwise `false`.
//////////////////////////////////////////////////////
static inline bool Make7_canWin(const Make7 *const restrict _M7)
{
    const uint8_t OFF_TILES[3] = { Make7_count(_M7->avails, _M7->turn, 0), Make7_count(_M7->avails, _M7->turn, 1), Make7_count(_M7->avails, _M7->turn, 2) };

    Make7 check7 = *_M7;

#ifdef __clang__
    #pragma clang loop unroll(enable)
#elifdef __GNUC__
    #pragma GCC unroll 3
#endif
    for (uint8_t tileIndex = 3; tileIndex--;)
    {
        for (uint64_t open12Mask = (_M7->mask + MAKE7_BOT) & MAKE7_ALL & (tileIndex == 2 ? MAKE7_THREES_MASK : MAKE7_ALL); open12Mask && OFF_TILES[tileIndex];)
        {
            const uint64_t TILE_MASK = open12Mask & -open12Mask;

            Make7_drop(&check7, tileIndex, stdc_trailing_zeros_ull(TILE_MASK) >> 3);

            if (Make7_targetSum(&check7))
            {
                return true;
            }

            check7 = *_M7;
            open12Mask ^= TILE_MASK;
        }
    }

    return false;
}

//////////////////////////////////////////////////////
/// @brief  Tests if a player has insufficient tiles.
/// @param  _M7
//////////////////////////////////////////////////////
static inline bool Make7_noMoreTiles(const Make7 *const restrict _M7)
{
    const uint8_t OFF_TILES[3] = { Make7_count(_M7->avails, _M7->turn, 0), Make7_count(_M7->avails, _M7->turn, 1), Make7_count(_M7->avails, _M7->turn, 2) };

    // Do they (players) have 1 and 2 tiles left?
    // Yes: Play it through until the latter condition is met.
    // No: Does the grid state allow droppable 3 tiles? If so, they have a move.
    return !(OFF_TILES[0] || OFF_TILES[1] || (OFF_TILES[2] && ((_M7->mask + MAKE7_BOT) & MAKE7_THREES_MASK)));
}

////////////////////////////////////////
/// @brief  Is the game of Make 7 over?
////////////////////////////////////////
static inline bool Make7_over(const Make7 *const restrict _M7)
{
    return Make7_targetSum(_M7) || Make7_noMoreTiles(_M7) || Make7_full(_M7);
}

///////////////////////////////////////////////////////
/// @brief  Announces the winner of the Make 7 game.
/// @param  _M7
/// @return `-1`: Ongoing;`0`: Draw; `1`: P1; `2`: P2.
///////////////////////////////////////////////////////
static inline int Make7_winner(const Make7 *const restrict _M7)
{
    return Make7_targetSum(_M7) ? (!(Make7_moves(_M7) & 1) ? 2 : 1) : -!Make7_noMoreTiles(_M7);
}

/////////////////////////////////////////////////////////
/// @brief  Horizontal symmetry test for a Make 7 board.
/// @param  _M7
/// @return `true` if symmetric; otherwise `false`.
/////////////////////////////////////////////////////////
bool Make7_symmetric(const Make7 *const restrict _M7)
{
    const uint64_t PL = _M7->side;
    const uint64_t OP = PL ^ _M7->mask;
    constexpr uint8_t MAKE7_MID_COL = MAKE7_SIZE / 2;

    for (uint8_t c = 0; c < MAKE7_MID_COL; c++)
    {
        const uint8_t LEFT_COL_BIT = c * MAKE7_SIZE_P1;
        const uint8_t RIGHT_COL_BIT = (MAKE7_SIZE_M1 - c) * MAKE7_SIZE_P1;

        const uint64_t PL_COL_LEFT = (PL & (MAKE7_COL_MASK << LEFT_COL_BIT)) >> LEFT_COL_BIT;
        const uint64_t OP_COL_LEFT = (OP & (MAKE7_COL_MASK << LEFT_COL_BIT)) >> LEFT_COL_BIT;
        const uint64_t T1_COL_LEFT = (_M7->tile1 & (MAKE7_COL_MASK << LEFT_COL_BIT)) >> LEFT_COL_BIT;
        const uint64_t T2_COL_LEFT = (_M7->tile2 & (MAKE7_COL_MASK << LEFT_COL_BIT)) >> LEFT_COL_BIT;

        const uint64_t PL_COL_RIGHT = (PL >> RIGHT_COL_BIT) & MAKE7_COL_MASK;
        const uint64_t OP_COL_RIGHT = (OP >> RIGHT_COL_BIT) & MAKE7_COL_MASK;
        const uint64_t T1_COL_RIGHT = (_M7->tile1 >> RIGHT_COL_BIT) & MAKE7_COL_MASK;
        const uint64_t T2_COL_RIGHT = (_M7->tile2 >> RIGHT_COL_BIT) & MAKE7_COL_MASK;

        if (PL_COL_LEFT != PL_COL_RIGHT || OP_COL_LEFT != OP_COL_RIGHT || T1_COL_LEFT != T1_COL_RIGHT || T2_COL_LEFT != T2_COL_RIGHT)
        {
            return false;
        }
    }

    return true;
}

///////////////////////////////////////
/// @brief  Make 7 partial key helper.
/// @note   Full key is 105 bits.
///////////////////////////////////////
static inline uint64_t Make7_partKey(const Make7 *const restrict _M7)
{
    return _M7->side + _M7->mask;
}

///////////////////////////////////////////////////////////
/// @brief  Make 7 lock function for transposition tables.
///////////////////////////////////////////////////////////
static inline uint64_t Make7_lock(const Make7 *const restrict _M7)
{
    const uint64_t LOCK_A = SplitMix64_finalize(_M7->side + _M7->mask + MAKE7_SM_SALT);
    const uint64_t LOCK_B = SplitMix64_finalize(_M7->tile1 + MAKE7_T1_SALT);
    const uint64_t LOCK_C = SplitMix64_finalize(_M7->tile2 + MAKE7_T2_SALT);

    return LOCK_A ^ LOCK_B ^ LOCK_C;
}

///////////////////////////////////////////////
/// @brief  Reverses (mirrors) a Make 7 board.
/// @param  _B
///////////////////////////////////////////////
static inline uint64_t Make7_reverse(const uint64_t _B)
{
    uint64_t rev = 0;

    for (uint8_t i = 0;; i++)
    {
        const uint8_t L_HALF = MAKE7_SIZE_P1 * i;
        const uint8_t R_HALF = MAKE7_SIZE_P1 * (MAKE7_SIZE_M1 - i);
        const int8_t R_M_L = R_HALF - L_HALF;

        if (R_M_L < 0)
        {
            return rev;
        }

        rev |= (_B & MAKE7_ALL_COL_MASK << L_HALF) << R_M_L;
        rev |= (_B & MAKE7_ALL_COL_MASK << R_HALF) >> R_M_L;
    }
}

///////////////////////////////////////////////////////////////////////////
/// @brief  Make 7 board canonicalization utility, not unlike Connect 4's.
/// @param  _B
///////////////////////////////////////////////////////////////////////////
static inline uint64_t Make7_canonicalize(const uint64_t _B)
{
    const uint64_t REV_M7 = Make7_reverse(_B);

    return _B < REV_M7 ? _B : REV_M7;
}

/////////////////////////////////////////////////////////////////
/// @brief          Processes user input and play a Make 7 move.
/// @param _m7      Unaliased pointer to the Make 7 state.
/// @param _INPUT   Letter (column) and number (tile) string.
/// @return         `true` if successful; otherwise `false`.
/////////////////////////////////////////////////////////////////
static inline bool Make7_parse(Make7 *const restrict _m7, const char _INPUT[restrict static 2])
{
    const uint8_t TILE_TOKEN = _INPUT[0] - '1';
    const uint8_t COL_TOKEN = toupper(_INPUT[1]) - 'A';

    if (TILE_TOKEN <= 2 && COL_TOKEN < MAKE7_SIZE && Make7_droppable(_m7, TILE_TOKEN, COL_TOKEN))
    {
        Make7_drop(_m7, TILE_TOKEN, COL_TOKEN);

        return true;
    }

    return false;
}

#endif // MAKE7_H //
