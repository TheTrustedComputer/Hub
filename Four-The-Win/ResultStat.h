/*
 *  Author: 2026- TheTrustedComputer
 *
 *  A compact structure for storing the result of a game tree search as colored text.
 *
 *  For example, if a player has a forced win in ten plies, it is formatted as a green "W10".
 *  Similar formatting applies to losses, which are red, but draws have no counter and are yellow.
 *  It incorporates explored positions, search speed, and elapsed time from NegaScout.
 */

#ifndef RESULT_H
#define RESULT_H

#define RESULT_NULL (Result) { .wdl = NULL_CHAR, .dtw = 0 }
#define RESULT_WIN (Result) { .wdl = WIN_CHAR, .dtw = 0 }
#define RESULT_DRAW (Result) { .wdl = DRAW_CHAR }
#define RESULT_LOSS (Result) { .wdl = LOSS_CHAR, .dtw = 0 }

typedef enum : char
{
    NULL_CHAR = '?', WIN_CHAR = 'W', DRAW_CHAR = 'D', LOSS_CHAR = 'L'
}
ResultChar;

#pragma pack(push, 1)

typedef struct
{
    ResultChar wdl;
    uint16_t dtw;
    uint64_t nodes, speed;
    double time;
}
Result;

#pragma pack(pop)

////////////////////////////////////////////////////////////
/// @brief          Prints a result to standard output.
/// @param  _RESULT Result to print.
/// @param  _BEST_R Best result; bold when == `_RESULT`.
/// @param  _DRAW_D Whether to write "DRAW" (F) or "D" (T).
/// @param  _STATS  Include Negamax search statistics?
/// @note           Uses colored text for easy reading.
/// @details        Red = Loss; Yellow = Draw; Green = Win
////////////////////////////////////////////////////////////
static inline void Result_print(const Result _RESULT, const Result *const restrict _BEST_R, const bool _DRAW_D, const bool _STATS)
{
    switch (_RESULT.wdl)
    {
    default:
        printf("\e[2m__\e[0m");
        break;
    case DRAW_CHAR:
        if (_BEST_R && _BEST_R->wdl == _RESULT.wdl)
        {
            _DRAW_D ? printf("\e[1;93m%c\e[0m", _RESULT.wdl) : printf("\e[1;93m%s\e[0m", FTW_STR_DRAW);
        }
        else
        {
            _DRAW_D ? printf("\e[0;33m%c\e[0m", _RESULT.wdl) : printf("\e[0;33m%s\e[0m", FTW_STR_DRAW);
        }
        break;
    case WIN_CHAR:
        if (_BEST_R && _BEST_R->wdl == _RESULT.wdl && _BEST_R->dtw == _RESULT.dtw)
        {
            _RESULT.dtw ? printf("\e[1;92m%c%u\e[0m", _RESULT.wdl, _RESULT.dtw) : printf("\e[1;92m%s\e[0m", FTW_STR_WIN);
        }
        else
        {
            _RESULT.dtw ? printf("\e[0;32m%c%u\e[0m", _RESULT.wdl, _RESULT.dtw) : printf("\e[0;32m%s\e[0m", FTW_STR_WIN);
        }
        break;
    case LOSS_CHAR:
        if (_BEST_R && _BEST_R->wdl == _RESULT.wdl && _BEST_R->dtw == _RESULT.dtw)
        {
            _RESULT.dtw ? printf("\e[1;91m%c%u\e[0m", _RESULT.wdl, _RESULT.dtw) : printf("\e[1;91m%s\e[0m", FTW_STR_LOSS);
        }
        else
        {
            _RESULT.dtw ? printf("\e[0;31m%c%u\e[0m", _RESULT.wdl, _RESULT.dtw) : printf("\e[0;31m%s\e[0m", FTW_STR_LOSS);
        }
        break;

    }

    if (_STATS)
    {
        printf(" %" PRIu64 " %" PRIu64 " %.9f\n", _RESULT.nodes, _RESULT.speed, _RESULT.time);
    }
}

//////////////////////////////////////////////////////
/// @brief  Increments a result by a ply (half-move).
/// @param  _r
/// @note   Assumes an alternating two-player game.
//////////////////////////////////////////////////////
static inline void Result_increment(Result *const restrict _r)
{
    switch (_r->wdl)
    {
    case WIN_CHAR:
        _r->wdl = LOSS_CHAR;
        _r->dtw++;
        break;
    case LOSS_CHAR:
        _r->wdl = WIN_CHAR;
        _r->dtw++;
    default:
        break;
    }
}

////////////////////////////////////////////////////
/// @brief  Converts the minimax value to a result.
/// @param  _VALUE
/// @param  _NODES
/// @param  _TIME
////////////////////////////////////////////////////
static inline Result Result_fromNegaScout(const int16_t _VALUE, const uint64_t _NODES, const double _TIME)
{
    return (Result)
    {
        .wdl = _VALUE > 0 ? WIN_CHAR : _VALUE < 0 ? LOSS_CHAR : DRAW_CHAR,
        .dtw = (abs(_VALUE ) - 1) * (bool)(_VALUE),
        .nodes = _NODES,
        .speed = _NODES / _TIME,
        .time = _TIME
    };
}

//////////////////////////////////////////////////////
/// @brief  Gets the best result from a result array.
/// @param  _R_ARR
/// @param  _R_LEN
//////////////////////////////////////////////////////
static inline Result Result_best(const Result _R_ARR[const restrict], const size_t _R_LEN)
{
    Result bestR = _R_ARR[0];
    ResultChar tierR = bestR.wdl;

    for (size_t i = 1; i < _R_LEN;)
    {
        switch (tierR)
        {
        case LOSS_CHAR:
            switch (_R_ARR[i].wdl)
            {
            case LOSS_CHAR:
                if (_R_ARR[i].dtw > bestR.dtw)
                {
                    bestR = _R_ARR[i];
                }
            default:
                break;
            case DRAW_CHAR:
            case WIN_CHAR:
                bestR = _R_ARR[i];
                tierR = (_R_ARR[i].wdl == DRAW_CHAR) ? DRAW_CHAR : WIN_CHAR;
                continue;
            }
            break;
        case DRAW_CHAR:
            if (_R_ARR[i].wdl == WIN_CHAR)
            {
                bestR = _R_ARR[i];
                tierR = WIN_CHAR;
                continue;
            }
            break;
        case WIN_CHAR:
            if (_R_ARR[i].wdl == WIN_CHAR && _R_ARR[i].dtw < bestR.dtw)
            {
                bestR = _R_ARR[i];
            }
            break;
        default:
            bestR = _R_ARR[i];
            tierR = _R_ARR[i].wdl;
            break;
        }

        i++;
    }

    return bestR;
}

#ifdef FTW_SQLITE
/////////////////////////////////////////////////////////////////
/// @brief  Recovers a result from a 16-bit signed scalar value.
/// @param  _s
/////////////////////////////////////////////////////////////////
static inline Result Result_fromScalar(int16_t _s)
{
    _s |= _s >= 128 && BOARD_AREA < 128 && (C4_variant != CONNECT4_POPOUT && C4_variant != CONNECT4_POP10) ? 0xff00 : 0x0;

    return _s > 0 ? (Result) { .wdl = WIN_CHAR, .dtw = _s - 1 } : _s < 0 ? (Result) { .wdl = LOSS_CHAR, .dtw = -_s - 1 } : RESULT_DRAW;
}

///////////////////////////////////////////////////////////////
/// @brief  Compacts a result to a 16-bit signed scalar value.
/// @param  _R
///////////////////////////////////////////////////////////////
static inline int16_t Result_toScalar(const Result *const restrict _R)
{
    switch (_R->wdl)
    {
    case WIN_CHAR:
        return _R->dtw + 1;
    case DRAW_CHAR:
    default:
        return 0;
    case LOSS_CHAR:
        return (-_R->dtw - 1) & (BOARD_AREA < 128 && C4_variant != CONNECT4_POPOUT && C4_variant != CONNECT4_POP10 ? 0xff : 0xffff);
    }
}

////////////////////////////////////////
/// @brief  Expands a BLOB to a result.
/// @param  _BLOB
////////////////////////////////////////
static inline Result Result_fromBLOB(const void *const restrict _BLOB)
{
    return _BLOB ? Result_fromScalar(*(const int16_t *const restrict)(_BLOB)) : RESULT_DRAW;
}
#endif

#endif // RESULT_H //
