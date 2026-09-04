/*
 *  Author: 2025- TheTrustedComputer
 *
 *  SplitMix is a popular and widely used pseudo-random number generator.
 *  It is noted as the default generator for Java's SplittableRandom class.
 */

#ifndef SPLITMIX_H
#define SPLITMIX_H

#include <stdint.h>

static constexpr uint32_t GOLDEN_RATIO_32 = 0x9e3779b9;
static constexpr uint64_t GOLDEN_RATIO_64 = 0x9e3779b97f4a7c15;

//////////////////////////////////////////////////////////////
/// @brief      Auxiliary finalizer for the 32-bit generator.
/// @param  _z  The value pointed to by the buffer.
/// @return     A 32-bit pseudorandom number.
/// @details    `https://github.com/skeeto/hash-prospector`
//////////////////////////////////////////////////////////////
uint32_t SplitMix32_finalize(uint32_t _z)
{
    _z = (_z ^ _z >> 16) * 0x21f0aaad;
    _z = (_z ^ _z >> 15) * 0x735a2d97;

    return _z ^ _z >> 15;
}

//////////////////////////////////////////////////////////////
/// @brief      Auxiliary finalizer for the 64-bit generator.
/// @param  _z  The value pointed to by the buffer.
/// @return     A 64-bit pseudorandom number.
//////////////////////////////////////////////////////////////
uint64_t SplitMix64_finalize(uint64_t _z)
{
    _z = (_z ^ _z >> 30) * 0xbf58476d1ce4e5b9;
    _z = (_z ^ _z >> 27) * 0x94d049bb133111eb;

    return _z ^ _z >> 31;
}

/////////////////////////////////////////////////////////////////////
/// @brief      Rolls the next number from the SplitMix32 generator.
/// @param  _z  Unaliased pointer to a 32-bit buffer.
/////////////////////////////////////////////////////////////////////
uint32_t SplitMix32(uint32_t *const restrict _z)
{
    return SplitMix32_finalize((*_z += GOLDEN_RATIO_32));
}

/////////////////////////////////////////////////////////////////////
/// @brief      Rolls the next number from the SplitMix64 generator.
/// @param  _z  Unaliased pointer to a 64-bit buffer.
/////////////////////////////////////////////////////////////////////
uint64_t SplitMix64(uint64_t *const restrict _z)
{
    return SplitMix64_finalize((*_z += GOLDEN_RATIO_64));
}

#endif // SPLITMIX_H //
