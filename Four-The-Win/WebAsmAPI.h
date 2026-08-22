#ifndef WEBASMAPI_H
#define WEBASMAPI_H

uint8_t FourTheWin_C4_cols(void)
{
    return UI_cols;
}

uint8_t FourTheWin_C4_rows(void)
{
    return UI_rows;
}

void FourTheWin_C4_prepare(void)
{
    Connect4_prepare(UI_cols, UI_rows);
}

void FourTheWin_C4_init(void)
{
    Connect4_init(&UI_c4);
    C4_variant == CONNECT4_POP10 ? Connect4_pop10_reset(&UI_c4, &UI_p10) : FTW_VOID_NOP;
    Connect4_funcPtrs_init();
    Connect4_globals_init();
}

void FourTheWin_C4_reset(void)
{
    Connect4_reset(&UI_c4);
}

int FourTheWin_C4_status(void)
{
    return C4_variant == CONNECT4_POP10 ? Connect4_pop10_winner(&UI_p10) : Connect4_winner(&UI_c4);
}

bool FourTheWin_C4_play(const char *restrict _str)
{
    while (*_str)
    {
        if (!Connect4_parse(&UI_c4, *_str++))
        {
            return false;
        }
    }

    return true;
}

void FourTheWin_C4_undo(void)
{
    UI_c4.plies ? Connect4_unplay(&UI_c4) : FTW_VOID_NOP;
}

Board FourTheWin_C4_playerMask(void)
{
    return UI_c4.side;
}

Board FourTheWin_C4_occupancyMask(void)
{
    return UI_c4.mask;
}

uint8_t FourTheWin_C4_owner(const uint8_t _COL, const uint8_t _ROW)
{
    const Board P_CELL = UI_c4.side & (BOARD(1) << (ROWS_P1 * _COL + _ROW));
    const Board O_CELL = UI_c4.mask & (BOARD(1) << (ROWS_P1 * _COL + _ROW));
    const bool TURN = UI_c4.plies & 1;

    return P_CELL ? (TURN ? 2 : 1) : (O_CELL ? (TURN ? 1 : 2) : 0);
}

uint8_t FourTheWin_varID(void)
{
    return C4_variant;
}

const char *FourTheWin_varName(void)
{
    return FTW_STR_VARIANTS[C4_variant];
}

#endif // WEBASMAPI_H
