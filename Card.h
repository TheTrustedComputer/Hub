/*
 *  Author: 2025- TheTrustedComputer
 *
 *  A header file that describes the English pattern of French-suited playing cards.
 *  Each card in a standard 52-card deck is uniquely identified by its suit and rank.
 *
 *  We utilize 6 bits to represent each card: 4 bits => rank, 2 bits => suit.
 *  Suit - 00 = Club; 01 = Diamond; 10 = Heart; 11 = Spade
 *  Rank - 0000 = Ace; 0001 = Two; ...; 1011 = Queen; 1100 = King
 *
 *  This design is memory efficient; a single byte is enough to hold every card detail.
 *  Bits 0-3 store the rank, and bits 4-5 carry the suit; the rest is unused (x): xxSSRRRR
 *
 *  Encoding and decoding are done using simple bitwise operations.
 *  Below is an example encoding for the King of Spades:
 *  Suit = 11
 *  Rank = 1100
 *  Binary = 111100; Hex = 0x3C
 */

#ifndef CARD_H
#define CARD_H

#include <stdint.h>
#include <stdio.h>

#if __STDC_VERSION__ < 202311l
    #include <stdbool.h>
    #define constexpr const
#endif

static constexpr uint8_t CARD_BITS = 6;
static constexpr uint8_t CARD_MASK = 63;
static constexpr uint8_t NUM_RANKS = 13;
static constexpr uint8_t NUM_SUITS = 4;
static constexpr uint8_t RANK_MASK = 15;
static constexpr uint8_t RANK_COUNT_BITS = 3;
static constexpr uint8_t RANK_COUNT_MASK = 7;
static constexpr uint8_t RANK_NONE = INT8_MAX;
static constexpr uint8_t SUIT_COUNT_BITS = RANK_COUNT_BITS;
static constexpr uint8_t SUIT_COUNT_MASK = RANK_COUNT_MASK;
static constexpr uint8_t SUIT_SHIFT = NUM_SUITS;

typedef enum : uint8_t
{
    CLUB, DIAMOND, HEART, SPADE
}
Suit;

typedef enum : int8_t // signed comparison
{
    ACE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING, JOKER
}
Rank;

typedef uint8_t Card;

/////////////////////////////////////////////////
/// @brief  Compares two ranks with Aces LOW.
/// @param  _R0
/// @param  _R1
/// @note   To pass the function into `qsort()`.
/////////////////////////////////////////////////
int8_t Rank_AL_cmp(const Rank _R0, const Rank _R1)
{
    return _R0 - _R1;
}

/////////////////////////////////////////////////
/// @brief  Compares two ranks with Aces HIGH.
/// @param  _R0
/// @param  _R1
/// @note   Jokers are ranked higher than Kings.
///         To pass the function into `qsort()`.
/////////////////////////////////////////////////
int8_t Rank_AH_cmp(const Rank _R0, const Rank _R1)
{
    const Rank R0 = _R0 ? _R1 - 1 : JOKER;
    const Rank R1 = _R0 ? _R1 - 1 : JOKER;

    return R0 - R1;
}

///////////////////////////////////////////////
/// @brief  Determines if a suit has red pips.
/// @param  _S
/// @return `true` if red; otherwise `false`.
///////////////////////////////////////////////
bool Suit_red(const Suit _S)
{
    return _S == DIAMOND || _S == HEART;
}

/////////////////////////////////////////////////
/// @brief  Determines if a suit has black pips.
/// @param  _S
/// @return `true` if black; otherwise `false`.
/////////////////////////////////////////////////
bool Suit_black(const Suit _S)
{
    return _S == CLUB || _S == SPADE;
}

///////////////////////////////////////////////////////////
/// @brief  Makes a card from a rank `_R` and a suit `_S`.
/// @param  _R
/// @param  _S
///////////////////////////////////////////////////////////
Card Card_make(const Rank _R, const Suit _S)
{
    return _R | _S << SUIT_SHIFT;
}

////////////////////////////////////////////////
/// @brief  Extracts the rank from a card `_C`.
/// @param  _C
////////////////////////////////////////////////
Rank Card_rank(const Card _C)
{
    return _C & RANK_MASK;
}

///////////////////////////////////////////
/// @brief  Extracts the suit from a card.
/// @param  _C
///////////////////////////////////////////
Suit Card_suit(const Card _C)
{
    return _C >> SUIT_SHIFT;
}

////////////////////////////////////////////////////////
/// @brief  Prints a formatted card to standard output.
////////////////////////////////////////////////////////
void Card_print(const Card _C)
{
    const Suit SUIT = Card_suit(_C);
    const Rank RANK = Card_rank(_C);

    printf("\e[7;1;"); // Invert and bold

    if (RANK == JOKER)
    {
        printf("44m"); // Blue Jokers
    }
    else if (Suit_black(SUIT))
    {
        printf("40m");
    }
    else if (Suit_red(SUIT))
    {
        printf("41m");
    }

    switch (RANK)
    {
    case ACE:
        printf("A");
        break;
    case TEN:
        printf("T");
        break;
    case JACK:
    case JOKER:
        printf("J");
        break;
    case QUEEN:
        printf("Q");
        break;
    case KING:
        printf("K");
        break;
    default:
        (RANK >= TWO && RANK <= NINE) ? printf("%d", RANK + 1) : printf("?");
        break;
    }

    if (RANK == JOKER)
    {
        printf("w"); // No suit; often a wild card
    }
    else
    {
        switch (SUIT)
        {
        case CLUB:
            printf("\u2663");
            break;
        case SPADE:
            printf("\u2660");
            break;
        case DIAMOND:
            printf("\u2666");
            break;
        case HEART:
            printf("\u2665");
            break;
        }
    }

    printf("\e[0m");
}

#endif // CARD_H //
