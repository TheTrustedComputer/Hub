/*
 *  Author: 2026- TheTrustedComputer
 *
 *  An in-memory ring buffer of recently solved positions.
 *
 *  After finding a solution, we will add the canonical position key to the ring.
 *  When the user queries it, we will search the ring for a matching key.
 *  This procedure avoids reanalyzing positions with known results.
 *  However, once the ring becomes full, we will overwrite the existing entry.
 *
 *  NOTE: All retained information will be lost once the program closes.
 *  For persistence storage, recompile with SQLite support.
 */

#ifndef RESULTRING_H
#define RESULTRING_H

static constexpr uint16_t RES_RING_SIZE = 256;

typedef struct
{
    Board keyA, keyB, keyC;
    Result res;
}
ResultRing;

static ResultRing ResultRing_buf[RES_RING_SIZE];
static uint8_t ResultRing_idx;

////////////////////////////////////////////////////////////////
/// @brief  Resets the result ring to something not meaningful.
////////////////////////////////////////////////////////////////
static inline void ResultRing_reset(void)
{
    ResultRing_idx = 0;

    for (uint16_t i = 0; i < RES_RING_SIZE; i++)
    {
        ResultRing *const restrict ring = &ResultRing_buf[i];

        ring->keyA = ring->keyB = ring->keyC = UINT64_MAX;
        ring->res = RESULT_NULL;
    }
}

/////////////////////////////////////////////////////////////////////
/// @brief  Linear search for a result in the result ring by key.
/// @return The corresponding result, or `RESULT_NULL` if not found.
/////////////////////////////////////////////////////////////////////
static inline Result ResultRing_query(const Board _KEY_A, const Board _KEY_B, const Board _KEY_C)
{
    for (uint16_t i = 0; i < RES_RING_SIZE; i++)
    {
        const ResultRing *const restrict RES_RING_ENTRY = &ResultRing_buf[i];

        if (RES_RING_ENTRY->keyA == _KEY_A && RES_RING_ENTRY->keyB == _KEY_B && RES_RING_ENTRY->keyC == _KEY_C)
        {
            return RES_RING_ENTRY->res;
        }
    }

    return RESULT_NULL;
}

///////////////////////////////////////////////////////////
/// @brief  Inserts a key and result into the result ring.
///////////////////////////////////////////////////////////
static inline void ResultRing_insert(const Board _KEY_A, const Board _KEY_B, const Board _KEY_C, const Result _RES)
{
    ResultRing *const restrict ringEntry = &ResultRing_buf[ResultRing_idx++];

    ringEntry->keyA = _KEY_A;
    ringEntry->keyB = _KEY_B;
    ringEntry->keyC = _KEY_C;
    ringEntry->res = _RES;

    ringEntry->res.nodes = ringEntry->res.speed = 0;
    ringEntry->res.time = 0.0;
}

#endif // RESULTRING_H //
