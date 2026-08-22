/*
 *  Author: 2025- TheTrustedComputer
 *  
 *  A compatibility layer that uses `__builtin_*()` functions for stdbit.h.
 *  This header was introduced in C23, but some compilers may not yet support it.
 */

#ifndef COMPAT_STDBIT_H
#define COMPAT_STDBIT_H

#if __has_builtin(__builtin_clz)
    #define COMPILER_HAS_CLZ
    #define Compat_clz(_A) __builtin_clz(_A)
#endif

#if __has_builtin(__builtin_clzl)
    #define COMPILER_HAS_CLZL
    #define Compat_clzl(_A) __builtin_clzl(_A)
#endif

#if __has_builtin(__builtin_clzll)
    #define COMPILER_HAS_CLZLL
    #define Compat_clzll(_A) __builtin_clzll(_A)
#endif

#if __has_builtin(__builtin_ctz)
    #define COMPILER_HAS_CTZ
    #define Compat_ctz(_A) __builtin_ctz(_A)
#endif

#if __has_builtin(__builtin_ctzl)
    #define COMPILER_HAS_CTZL
    #define Compat_ctzl(_A) __builtin_ctzl(_A)
#endif

#if __has_builtin(__builtin_ctzll)
    #define COMPILER_HAS_CTZLL
    #define Compat_ctzll(_A) __builtin_ctzll(_A)
#endif

#if __has_builtin(__builtin_popcount)
    #define COMPILER_HAS_POPCNT
    #define Compat_popcnt(_A) __builtin_popcount(_A)
#endif

#if __has_builtin(__builtin_popcountl)
    #define COMPILER_HAS_POPCNTL
    #define Compat_popcntl(_A) __builtin_popcountl(_A)
#endif

#if __has_builtin(__builtin_popcountll)
    #define COMPILER_HAS_POPCNTLL
    #define Compat_popcntll(_A) __builtin_popcountll(_A)
#endif

#ifndef COMPILER_HAS_CLZ
static inline unsigned Compat_clz(unsigned _a)
{
    unsigned lz = 0u;
    
#if __LP64__ == 1
    for (; !(_a & 0x80000000u); lz++, _a <<= 1u);
#else
    for (; !(_a & 0x8000u); lz++, _a <<= 1u);
#endif
    
    return lz;
}
#endif

#ifndef COMPILER_HAS_CLZL
static inline unsigned Compat_clzl(unsigned long _a)
{
    unsigned lz = 0u;
    
#if __LP64__ == 1
    for (; !(_a & 0x8000000000000000ul); lz++, _a <<= 1ul);
#else
    for (; !(_a & 0x80000000ul); lz++, _a <<= 1ul);
#endif
    
    return lz;
}
#endif

#ifndef COMPILER_HAS_CLZLL
static inline unsigned Compat_clzll(unsigned long long _a)
{
    unsigned lz = 0u;
    
    for (; !(_a & 0x8000000000000000ull); lz++, _a <<= 1ull);
    
    return lz;
}
#endif

#ifndef COMPILER_HAS_CTZ
static inline unsigned Compat_ctz(unsigned _a)
{
    unsigned tz = 0u;
    
    for (; !(_a & 1u); tz++, _a >>= 1u);
    
    return tz;
}
#endif

#ifndef COMPILER_HAS_CTZL
static inline unsigned Compat_ctzl(unsigned long _a)
{
    unsigned tz = 0u;
    
    for (; !(_a & 1ul); tz++, _a >>= 1ul);
    
    return tz;
}
#endif

#ifndef COMPILER_HAS_CTZLL
static inline unsigned Compat_ctzll(unsigned long long _a)
{
    unsigned tz = 0u;
    
    for (; !(_a & 1ull); tz++, _a >>= 1ull);
    
    return tz;
}
#endif

#if !(defined(COMPILER_HAS_POPCNT) && defined(COMPILER_HAS_POPCNTL) && defined(COMPILER_HAS_POPCNTLL))
static inline unsigned Compat_popcntll(unsigned long long _a)
{
    unsigned pc = 0u;
    
    for (; _a; pc++, _a &= _a - 1ull);
    
    return pc;
}

static inline unsigned Compat_popcnt(unsigned _a) { return Compat_popcntll(_a); }
static inline unsigned Compat_popcntl(unsigned long _a) { return Compat_popcntll(_a); }
#endif

static inline unsigned stdc_leading_zeros_uc(const unsigned char _A) [[unsequenced]] { return _A ? Compat_clz(_A) : 8u; }
static inline unsigned stdc_leading_zeros_us(const unsigned short _A) [[unsequenced]] { return _A ? Compat_clz(_A) : 16u; }
static inline unsigned stdc_leading_zeros_ull(const unsigned long long _A) [[unsequenced]] { return _A ? Compat_clzll(_A) : 64u; }

static inline unsigned stdc_leading_zeros_ui(const unsigned _A) [[unsequenced]]
{
#if __LP64__ == 1
    return _A ? Compat_clz(_A) : 32u;
#else
    return _A ? Compat_clz(_A) : 16u;
#endif
}

static inline unsigned stdc_leading_zeros_ul(const unsigned long _A) [[unsequenced]]
{
#if __LP64__ == 1
    return _A ? Compat_clzl(_A) : 64u;
#else
    return _A ? Compat_clzl(_A) : 32u;
#endif
}

#define stdc_leading_zeros(_A) _Generic(_A, \
    default: Compat_clz, \
    char: stdc_leading_zeros_uc, \
    unsigned char: stdc_leading_zeros_uc, \
    short: stdc_leading_zeros_us, \
    unsigned short: stdc_leading_zeros_us, \
    int: stdc_leading_zeros_ui, \
    unsigned: stdc_leading_zeros_ui, \
    long: stdc_leading_zeros_ul, \
    unsigned long: stdc_leading_zeros_ul, \
    long long: stdc_leading_zeros_ull, \
    unsigned long long: stdc_leading_zeros_ull \
) (_A)

static inline unsigned stdc_trailing_zeros_uc(const unsigned char _A) [[unsequenced]] { return _A ? Compat_ctz(_A) : 8u; }
static inline unsigned stdc_trailing_zeros_us(const unsigned short _A) [[unsequenced]] { return _A ? Compat_ctz(_A) : 16u; }
static inline unsigned stdc_trailing_zeros_ull(const unsigned long long _A) [[unsequenced]] { return _A ? Compat_ctzll(_A) : 64u; }

static inline unsigned stdc_trailing_zeros_ui(const unsigned _A) [[unsequenced]]
{
#if __LP64__ == 1
    return _A ? Compat_ctz(_A) : 32u;
#else
    return _A ? Compat_ctz(_A) : 16u;
#endif
}

static inline unsigned stdc_trailing_zeros_ul(const unsigned long _A) [[unsequenced]]
{
#if __LP64__ == 1
    return _A ? Compat_ctzl(_A) : 64u;
#else
    return _A ? Compat_ctzl(_A) : 32u;
#endif
}

#define stdc_trailing_zeros(_A) _Generic(_A, \
    default: Compat_ctz, \
    char: stdc_trailing_zeros_uc, \
    unsigned char: stdc_trailing_zeros_uc, \
    short: stdc_trailing_zeros_us, \
    unsigned short: stdc_trailing_zeros_us, \
    int: stdc_trailing_zeros_ui, \
    unsigned: stdc_trailing_zeros_ui, \
    long: stdc_trailing_zeros_ul, \
    unsigned long: stdc_trailing_zeros_ul, \
    long long: stdc_trailing_zeros_ull, \
    unsigned long long: stdc_trailing_zeros_ull \
) (_A)

static inline unsigned stdc_count_ones_uc(const unsigned char _A) [[unsequenced]] { return Compat_popcnt(_A); }
static inline unsigned stdc_count_ones_us(const unsigned short _A) [[unsequenced]] { return Compat_popcnt(_A); }
static inline unsigned stdc_count_ones_ui(const unsigned _A) [[unsequenced]] { return Compat_popcnt(_A); }
static inline unsigned stdc_count_ones_ul(const unsigned long _A) [[unsequenced]] { return Compat_popcntl(_A); }
static inline unsigned stdc_count_ones_ull(const unsigned long long _A) [[unsequenced]] { return Compat_popcntll(_A); }

#define stdc_count_ones(_A) _Generic(_A, \
    default: Compat_popcnt, \
    char: stdc_count_ones_uc, \
    unsigned char: stdc_count_ones_uc, \
    short: stdc_count_ones_us, \
    unsigned short: stdc_count_ones_us, \
    int: stdc_count_ones_ui, \
    unsigned: stdc_count_ones_ui, \
    long: stdc_count_ones_ul, \
    unsigned long: stdc_count_ones_ul, \
    long long: stdc_count_ones_ull, \
    unsigned long long: stdc_count_ones_ull \
) (_A)

#endif // COMPAT_STDBIT_H //
