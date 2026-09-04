/*
 *  Author: 2026- TheTrustedComputer
 *
 *  MurmurHash is a fast, non-cryptographic hash function created by Austin Appleby.
 *  The original author placed the code in the public domain, so any developer may use it freely.
 *
 *  This header rewrites `MurmurHash3_x64_128`, the 128-bit x86-64 version of MurmurHash3.
 *  Although considered outdated by modern standards, it stays relevant in certain fields.
 */

#ifndef MURMUR3_H
#define MURMUR3_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static constexpr uint64_t MURMUR3_C1 = 0x87c37b91114253d5;
static constexpr uint64_t MURMUR3_C2 = 0x4cf5ad432745937f;

typedef struct
{
    uint64_t h1, h2;
}
Murmur128;

/////////////////////////////////////////////////////
/// @brief  Rotates a 64-bit `_N` left by `_K` bits.
/// @param  _N
/// @param  _K
/////////////////////////////////////////////////////
static inline uint64_t Murmur3_rotl(const uint64_t _N, const uint8_t _K)
{
#ifdef __clang__
    return __builtin_rotateleft64(_N, _K);
#elifdef __GNUC__
    return __builtin_stdc_rotate_left(_N, _K);
#else
    return _N << _K | _N >> (64 - _K);
#endif
}

////////////////////////////////////////////
/// @brief  MurmurHash3's 64-bit finalizer.
/// @param  _Z
////////////////////////////////////////////
static inline uint64_t Murmur3_fmix(uint64_t _z)
{
    _z ^= _z >> 33;
    _z *= 0xff51afd7ed558ccd;
    _z ^= _z >> 33;
    _z *= 0xc4ceb9fe1a85ec53;

    return _z ^ _z >> 33;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief          The 128-bit MurmurHash3 function for x86-64 architectures.
/// @param  _KEY    An unaliased pointer to a data buffer.
/// @param  _SIZE   The length of the data buffer.
/// @param  _SEED   A 32-bit number used to seed the hash.
/// @return         A 128-bit hash composed of two 64-bit halves (Murmur128).
///////////////////////////////////////////////////////////////////////////////
Murmur128 Murmur3_x64_128bits(const void *const restrict _KEY, const size_t _SIZE, const uint32_t _SEED)
{
    const uint8_t *const restrict KEY = _KEY;
    const size_t BLKS = _SIZE >> 4;

    uint64_t h1 = _SEED, h2 = _SEED, k1, k2;

    for (size_t i = 0; i < BLKS; i++)
    {
        const void *const restrict BLK = KEY + (i << 4);

        memcpy(&k1, BLK, sizeof(k1));
        memcpy(&k2, BLK + 8, sizeof(k2));

        k1 *= MURMUR3_C1;
        k1 = Murmur3_rotl(k1, 31);
        k1 *= MURMUR3_C2;
        h1 ^= k1;

        h1 = Murmur3_rotl(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= MURMUR3_C2;
        k2 = Murmur3_rotl(k2, 33);
        k2 *= MURMUR3_C1;
        h2 ^= k2;

        h2 = Murmur3_rotl(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    const uint8_t *const restrict TAIL = KEY + (BLKS << 4);

    k1 = k2 = 0;

    switch (_SIZE & 15)
    {
    case 15:
        k2 ^= (uint64_t)(TAIL[14]) << 48;
        [[fallthrough]];
    case 14:
        k2 ^= (uint64_t)(TAIL[13]) << 40;
        [[fallthrough]];
    case 13:
        k2 ^= (uint64_t)(TAIL[12]) << 32;
        [[fallthrough]];
    case 12:
        k2 ^= (uint64_t)(TAIL[11]) << 24;
        [[fallthrough]];
    case 11:
        k2 ^= (uint64_t)(TAIL[10]) << 16;
        [[fallthrough]];
    case 10:
        k2 ^= (uint64_t)(TAIL[9]) << 8;
        [[fallthrough]];
    case 9:
        k2 ^= (uint64_t)(TAIL[8]);
        k2 *= MURMUR3_C2;
        k2 = Murmur3_rotl(k2, 33);
        k2 *= MURMUR3_C1;
        h2 ^= k2;
        [[fallthrough]];
    case 8:
        k1 ^= (uint64_t)(TAIL[7]) << 56;
        [[fallthrough]];
    case 7:
        k1 ^= (uint64_t)(TAIL[6]) << 48;
        [[fallthrough]];
    case 6:
        k1 ^= (uint64_t)(TAIL[5]) << 40;
        [[fallthrough]];
    case 5:
        k1 ^= (uint64_t)(TAIL[4]) << 32;
        [[fallthrough]];
    case 4:
        k1 ^= (uint64_t)(TAIL[3]) << 24;
        [[fallthrough]];
    case 3:
        k1 ^= (uint64_t)(TAIL[2]) << 16;
        [[fallthrough]];
    case 2:
        k1 ^= (uint64_t)(TAIL[1]) << 8;
        [[fallthrough]];
    case 1:
        k1 ^= (uint64_t)(TAIL[0]);
        k1 *= MURMUR3_C1;
        k1 = Murmur3_rotl(k1, 31);
        k1 *= MURMUR3_C2;
        h1 ^= k1;
    case 0:
        break;
    }

    h1 ^= _SIZE;
    h2 ^= _SIZE;
    h1 += h2;
    h2 += h1;
    h1 = Murmur3_fmix(h1);
    h2 = Murmur3_fmix(h2);
    h1 += h2;
    h2 += h1;

    return (Murmur128) { h1, h2 };
}

#endif // MURMUR3_H //
