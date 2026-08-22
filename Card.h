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

constexpr uint8_t CARD_BITS = 6;
constexpr uint8_t CARD_MASK = 63;
constexpr uint8_t NUM_RANKS = 13;
constexpr uint8_t NUM_SUITS = 4;
constexpr uint8_t RANK_MASK = 15;
constexpr uint8_t RANK_COUNT_BITS = 3;
constexpr uint8_t RANK_COUNT_MASK = 7;
constexpr uint8_t RANK_NONE = INT8_MAX;
constexpr uint8_t SUIT_COUNT_BITS = RANK_COUNT_BITS;
constexpr uint8_t SUIT_COUNT_MASK = RANK_COUNT_MASK;
constexpr uint8_t SUIT_SHIFT = NUM_SUITS;

typedef enum : uint8_t
{
    CLUB, DIAMOND, HEART, SPADE
}
Suit;

typedef enum : int8_t
{
    ACE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING, JOKER
}
Rank;

typedef uint8_t Card;

/////////////////////////////////////////////////
/// @brief  Compares two ranks with Aces LOW.
/// @note   To pass the function into `qsort()`.
/////////////////////////////////////////////////
int8_t Rank_AL_cmp(const Rank _RANK0, const Rank _RANK1)
{
    return _RANK0 - _RANK1;
}

/////////////////////////////////////////////////
/// @brief  Compares two ranks with Aces HIGH.
/// @note   Jokers are ranked higher than Kings.
///         To pass the function into `qsort()`.
/////////////////////////////////////////////////
int8_t Rank_AH_cmp(const Rank _RANK0, const Rank _RANK1)
{
    const Rank R0 = _RANK0 ? _RANK0 - 1 : JOKER;
    const Rank R1 = _RANK1 ? _RANK1 - 1 : JOKER;

    return R0 - R1;
}

//////////////////////////////////////////////
/// @brief  Determines if a suit is red.
/// @return `true` if red; otherwise `false`.
//////////////////////////////////////////////
bool Suit_red(const Suit _SUIT)
{
    return _SUIT == DIAMOND || _SUIT == HEART;
}

////////////////////////////////////////////////
/// @brief  Determines if a suit is black.
/// @return `true` if black; otherwise `false`.
////////////////////////////////////////////////
bool Suit_black(const Suit _SUIT)
{
    return _SUIT == CLUB || _SUIT == SPADE;
}

/////////////////////////////////////////////////////////////////
/// @brief  Makes a card from a rank `_RANK` and a suit `_SUIT`.
/////////////////////////////////////////////////////////////////
Card Card_make(const Rank _RANK, const Suit _SUIT)
{
    return _RANK | (_SUIT << SUIT_SHIFT);
}

///////////////////////////////////////////
/// @brief  Extracts the rank from a card.
///////////////////////////////////////////
Rank Card_rank(const Card _CARD)
{
    return _CARD & RANK_MASK;
}

///////////////////////////////////////////
/// @brief  Extracts the suit from a card.
///////////////////////////////////////////
Suit Card_suit(const Card _CARD)
{
    return _CARD >> SUIT_SHIFT;
}

////////////////////////////////////////////////////////
/// @brief  Prints a formatted card to standard output.
////////////////////////////////////////////////////////
void Card_print(const Card _CARD)
{
    const Suit SUIT = Card_suit(_CARD);
    const Rank RANK = Card_rank(_CARD);

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
