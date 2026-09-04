/*
 *  Author: 2025- TheTrustedComputer
 *
 *  Reimplementation of Xoshiro256, a fast general-purpose pseudo-random number generator.
 *  All credits go to David Blackman and Sebastiano Vigna for creating it and SplitMix64.
 *  It has a period of 2^256 - 1, or roughly 1.1579208924 x 10^77 in scientific notation.
 *
 *  Also included in this header file is Xoshiro128, a 32-bit version of the generator.
 */

#ifndef XOSHIRO_H
#define XOSHIRO_H

#include <stdint.h>
#include <time.h>

typedef struct
{
    uint32_t s[4];
}
Xoshiro128;

typedef struct
{
    uint64_t s[4];
}
Xoshiro256;

////////////////////////////////////////////////////////////////
/// @brief          Initializes an Xoshiro128 PRNG with a seed.
/// @param  _xsr    Unaliased pointer to the Xoshiro128 struct.
/// @param  _SEED   A 32-bit number.
////////////////////////////////////////////////////////////////
static inline void Xoshiro128_seed(Xoshiro128 *const restrict _xsr, uint32_t _seed)
{
    do
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            _xsr->s[i] = SplitMix32(&_seed);
        }
    }
    while (!(_xsr->s[0] || _xsr->s[1] || _xsr->s[2] || _xsr->s[3]));
}

////////////////////////////////////////////////////////////////
/// @brief          Initializes an Xoshiro256 PRNG with a seed.
/// @param  _xsr    Unaliased pointer to the Xoshiro256 struct.
/// @param  _SEED   A 64-bit number.
////////////////////////////////////////////////////////////////
static inline void Xoshiro256_seed(Xoshiro256 *const restrict _xsr, uint64_t _seed)
{
    do
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            _xsr->s[i] = SplitMix64(&_seed);
        }
    }
    while (!(_xsr->s[0] || _xsr->s[1] || _xsr->s[2] || _xsr->s[3]));
}

/////////////////////////////////////////////////////////////////////////////
/// @brief  Rotates a 32-bit unsigned integer `_N` to the left by `_K` bits.
/// @param  _N
/// @param  _K
/////////////////////////////////////////////////////////////////////////////
static inline uint32_t Xoshiro128_rotl(const uint32_t _N, const uint8_t _K)
{
#ifdef __clang__
    return __builtin_rotateleft32(_N, _K);
#elifdef __GNUC__
    return __builtin_stdc_rotate_left(_N, _K);
#else
    return _N << _K | _N >> (32 - _K);
#endif
}

/////////////////////////////////////////////////////////////////////////////
/// @brief  Rotates a 64-bit unsigned integer `_N` to the left by `_K` bits.
/// @param  _N
/// @param  _K
/////////////////////////////////////////////////////////////////////////////
static inline uint64_t Xoshiro256_rotl(const uint64_t _N, const uint8_t _K)
{
#ifdef __clang__
    return __builtin_rotateleft64(_N, _K);
#elifdef __GNUC__
    return __builtin_stdc_rotate_left(_N, _K);
#else
    return _N << _K | _N >> (64 - _K);
#endif
}

//////////////////////////////////////////////////////////////////////
/// @brief          Helper function to initialize an Xoshiro128 PRNG.
/// @param  _xsr    Unaliased pointer to the Xoshiro128 struct.
//////////////////////////////////////////////////////////////////////
void Xoshiro128_init(Xoshiro128 *const restrict _xsr)
{
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);
    Xoshiro128_seed(_xsr, SplitMix32_finalize(ts.tv_sec * 1000000000 + ts.tv_nsec));
}

//////////////////////////////////////////////////////////////////////
/// @brief          Helper function to initialize an Xoshiro256 PRNG.
/// @param  _xsr    Unaliased pointer to the Xoshiro256 struct.
//////////////////////////////////////////////////////////////////////
void Xoshiro256_init(Xoshiro256 *const restrict _xsr)
{
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);
    Xoshiro256_seed(_xsr, ts.tv_sec * 1000000000 + ts.tv_nsec);
}

/////////////////////////////////////////////////////////////////
/// @brief          Rolls the next random number (Xoshiro128**).
/// @param  _xsr    Unaliased pointer to the Xoshiro128 struct.
/////////////////////////////////////////////////////////////////
uint32_t Xoshiro128ss_next(Xoshiro128 *const restrict _xsr)
{
    const uint32_t R = Xoshiro128_rotl(_xsr->s[1] * 5, 7) * 9;
    const uint32_t T = _xsr->s[1] << 9;

    _xsr->s[2] ^= _xsr->s[0];
    _xsr->s[3] ^= _xsr->s[1];
    _xsr->s[1] ^= _xsr->s[2];
    _xsr->s[0] ^= _xsr->s[3];
    _xsr->s[2] ^= T;
    _xsr->s[3] = Xoshiro128_rotl(_xsr->s[3], 11);

    return R;
}

/////////////////////////////////////////////////////////////////
/// @brief          Rolls the next random number (Xoshiro128++).
/// @param  _xsr    Unaliased pointer to the Xoshiro128 struct.
/////////////////////////////////////////////////////////////////
uint32_t Xoshiro128pp_next(Xoshiro128 *const restrict _xsr)
{
    const uint32_t R = Xoshiro128_rotl(_xsr->s[0] + _xsr->s[3], 7) + _xsr->s[0];
    const uint32_t T = _xsr->s[1] << 9;

    _xsr->s[2] ^= _xsr->s[0];
    _xsr->s[3] ^= _xsr->s[1];
    _xsr->s[1] ^= _xsr->s[2];
    _xsr->s[0] ^= _xsr->s[3];
    _xsr->s[2] ^= T;
    _xsr->s[3] = Xoshiro128_rotl(_xsr->s[3], 11);

    return R;
}

/////////////////////////////////////////////////////////////////
/// @brief          Rolls the next random number (Xoshiro256**).
/// @param  _xsr    Unaliased pointer to the Xoshiro256 struct.
/////////////////////////////////////////////////////////////////
uint64_t Xoshiro256ss_next(Xoshiro256 *const restrict _xsr)
{
    const uint64_t R = Xoshiro256_rotl(_xsr->s[1] * 5, 7) * 9;
    const uint64_t T = _xsr->s[1] << 17;

    _xsr->s[2] ^= _xsr->s[0];
    _xsr->s[3] ^= _xsr->s[1];
    _xsr->s[1] ^= _xsr->s[2];
    _xsr->s[0] ^= _xsr->s[3];
    _xsr->s[2] ^= T;
    _xsr->s[3] = Xoshiro256_rotl(_xsr->s[3], 45);

    return R;
}

///////////////////////////////////////////////////////////////////
/// @brief          Rolls the next random number (Xoshiro256**).
/// @param  _xsr    Unaliased pointer to the Xoshiro256 structure.
///////////////////////////////////////////////////////////////////
uint64_t Xoshiro256pp_next(Xoshiro256 *const restrict _xsr)
{
    const uint64_t R = Xoshiro256_rotl(_xsr->s[0] + _xsr->s[3], 23) + _xsr->s[0];
    const uint64_t T = _xsr->s[1] << 17;

    _xsr->s[2] ^= _xsr->s[0];
    _xsr->s[3] ^= _xsr->s[1];
    _xsr->s[1] ^= _xsr->s[2];
    _xsr->s[0] ^= _xsr->s[3];
    _xsr->s[2] ^= T;
    _xsr->s[3] = Xoshiro256_rotl(_xsr->s[3], 45);

    return R;
}

////////////////////////////////////////////////////////////////////////
/// @brief          Rolls the next number within [0, _N), Xoshiro128**.
/// @param  _xsr    Unaliased pointer to the Xoshiro128 structure.
/// @param  _N      The 32-bit upper bound of the range.
////////////////////////////////////////////////////////////////////////
uint32_t Xoshiro128ss_nextN(Xoshiro128 *const restrict _xsr, const uint32_t _N)
{
    return Xoshiro128ss_next(_xsr) * (uint64_t)(_N) >> 32;
}

////////////////////////////////////////////////////////////////////////
/// @brief          Rolls the next number within [0, _N), Xoshiro128++.
/// @param  _xsr    Unaliased pointer to the Xoshiro128 structure.
/// @param  _N      The 32-bit upper bound of the range.
////////////////////////////////////////////////////////////////////////
uint32_t Xoshiro128pp_nextN(Xoshiro128 *const restrict _xsr, const uint32_t _N)
{
    return Xoshiro128pp_next(_xsr) * (uint64_t)(_N) >> 32;
}

////////////////////////////////////////////////////////////////////////
/// @brief          Rolls the next number within [0, _N), Xoshiro256**.
/// @param  _xsr    Unaliased pointer to the Xoshiro256 structure.
/// @param  _N      The 64-bit upper bound of the range.
////////////////////////////////////////////////////////////////////////
uint64_t Xoshiro256ss_nextN(Xoshiro256 *const restrict _xsr, const uint64_t _N)
{
#ifdef __SIZEOF_INT128__
    return Xoshiro256ss_next(_xsr) * (__uint128_t)(_N) >> 64;
#else
    return Xoshiro256ss_next(_xsr) * (_BitInt(128))(_N) >> 64;
#endif
}

////////////////////////////////////////////////////////////////////////
/// @brief          Rolls the next number within [0, _N), Xoshiro256++.
/// @param  _xsr    Unaliased pointer to the Xoshiro256 structure.
/// @param  _N      The 64-bit upper bound of the range.
////////////////////////////////////////////////////////////////////////
uint64_t Xoshiro256pp_nextN(Xoshiro256 *const restrict _xsr, const uint64_t _N)
{
#ifdef __SIZEOF_INT128__
    return (Xoshiro256pp_next(_xsr) * (__uint128_t)(_N)) >> 64;
#else
    return (Xoshiro256pp_next(_xsr) * (_BitInt(128))(_N)) >> 64;
#endif
}

#endif // XOSHIRO_H //
