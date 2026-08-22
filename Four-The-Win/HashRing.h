/*
 *  Author: 2026- TheTrustedComputer
 *
 *  The Hash Ring, a small circular buffer of hashes for local repetition detection.
 */

#ifndef HASHRING_H
#define HASHRING_H

static constexpr uint8_t RING_SIZE = 13;

static uint64_t HashRing_buf[RING_SIZE];
static uint8_t HashRing_idx;

/////////////////////////////////////////////
/// @brief  Gets the next index in the ring.
/// @param  _IDX
/////////////////////////////////////////////
static inline uint8_t HashRing_nextIdx(const uint8_t _IDX)
{
    return (_IDX + 1) % RING_SIZE;
}

/////////////////////////////////////////////////
/// @brief  Gets the previous index in the ring.
/// @param  _IDX
/////////////////////////////////////////////////
static inline uint8_t HashRing_prevIdx(const uint8_t _IDX)
{
    return (_IDX + RING_SIZE - 1) % RING_SIZE;
}

///////////////////////////////////////////////////
/// @brief  Gets the repetition index in the ring.
/// @param  _IDX
///////////////////////////////////////////////////
static inline uint8_t HashRing_repeatIdx(const uint8_t _IDX)
{
    return _IDX + ((int8_t)(_IDX - 4) < 0) * RING_SIZE - 4;
}

//////////////////////////////////////////////////////////////////////
/// @brief  Checks if the ring contains a three-fold repetition.
//////////////////////////////////////////////////////////////////////
static inline bool HashRing_repeat(void)
{
    const uint8_t FIRST_IDX = HashRing_prevIdx(HashRing_idx);

    return HashRing_buf[FIRST_IDX] == HashRing_buf[HashRing_repeatIdx(HashRing_repeatIdx(FIRST_IDX))];
}

/////////////////////////////////////////////////////////////
/// @brief  Pushes a hash to the ring and updates the index.
/// @param  _HASH
/////////////////////////////////////////////////////////////
static inline void HashRing_push(const uint64_t _HASH)
{
    HashRing_buf[HashRing_idx] = _HASH;
    HashRing_idx = HashRing_nextIdx(HashRing_idx);
}

/////////////////////////////////////////////////
/// @brief  Pops the oldest hash from the ring.
/// @note   The index simply advances backwards.
/////////////////////////////////////////////////
static inline void HashRing_pop(void)
{
    HashRing_idx = HashRing_prevIdx(HashRing_idx);
}

//////////////////////////////////////////////
/// @brief  Resets the ring to unique values.
//////////////////////////////////////////////
static inline void HashRing_reset(void)
{
    HashRing_idx = 0;

    for (uint8_t i = 0; i < RING_SIZE; i++)
    {
        HashRing_buf[i] = i;
    }
}

#endif // HASHRING_H //
