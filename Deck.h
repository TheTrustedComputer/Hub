/*
 *  Author: 2025- TheTrustedComputer
 *
 *  The deck of playing cards using arrays as the container.
 *  It populates and shuffles from a set of 52 cards up to 5 jokers.
 */

#ifndef DECK_H
#define DECK_H

#include <stdint.h>

#if __STDC_VERSION__ < 202311l
    #define constexpr const
#endif

#ifndef CARD_H
    #include "Card.h"
#endif

#ifndef XOSHIRO_H
    #include "Xoshiro.h"
#endif

constexpr uint8_t DECK_TCNT = 57; // 52 cards + 5 jokers
constexpr uint8_t DECK_DCNT = 52; // After drawing 5 cards

typedef struct
{
    Card cards[DECK_TCNT];
    uint8_t count;
}
Deck;

//////////////////////////////////////////////////////////////////////
/// @brief          Populates a deck with cards plus optional jokers.
/// @param  _deck   Unaliased pointer to a deck to populate cards.
/// @param  _N_JK   Number of jokers to populate; must be at most 5.
//////////////////////////////////////////////////////////////////////
void Deck_populate(Deck *const restrict _deck, const uint8_t _N_JK)
{
    _deck->count = 0;

    for (Suit s = CLUB; s <= SPADE; s++)
    {
        for (Rank r = ACE; r <= KING; r++)
        {
            _deck->cards[_deck->count++] = Card_make(r, s);
        }
    }

    if (_N_JK <= 5)
    {
        for (uint8_t i = 0; i < _N_JK; i++)
        {
            _deck->cards[_deck->count++] = Card_make(JOKER, 0);
        }
    }
}

///////////////////////////////////////////////////////////////////////
/// @brief          Shuffles a deck by swapping cards with each other.
/// @param  _deck   Unaliased pointer to a deck to shuffle.
/// @note           Uses Xoshiro128++ for fast randomization.
///////////////////////////////////////////////////////////////////////
void Deck_shuffle(Deck *const restrict _deck)
{
    Xoshiro128 xsr128;
    Xoshiro128_init(&xsr128);

    for (uint8_t i = _deck->count; --i;)
    {
        const uint64_t CARD_POS = Xoshiro128pp_nextN(&xsr128, i + 1);
        const Card CARD_SHF = _deck->cards[i];

        _deck->cards[i] = _deck->cards[CARD_POS];
        _deck->cards[CARD_POS] = CARD_SHF;
    }
}

#endif // DECK_H //
