/*
 *  Author: 2025- TheTrustedComputer
 *
 *  A collection of enhanced library functions for working safely with dynamic memory and error checking.
 *  Some have been notorious for their insecure implementations, such as `scanf()` and `strcpy()`.
 *  The header file "RECSIO" stands for "REinCarnated String Input/Output", pronounced "wreck-see-oh".
 *  Originally intended for input/output and string functions, it has evolved to encompass many others.
 *
 *  It encourages defensive programming and efficient memory management using short lines of code.
 *  Memory corruption and pointer mishandling are the number one cause of bugs in C/C++ programs.
 *  If any function becomes part of the standard C library, it will supersede the ones here.
 *
 *  We will use "NULL" instead of "nullptr" for backward compatibility with older revisions.
 *  However, it is NOT designed for C++: the presence of `restrict` keywords, implicit casts, etc.
 *  Thus, we are targeting C99 and later. Any older C revision, such as C89, will not compile.
 *
 *  We cannot guarantee that these functions will prevent all classes of memory issues or problems.
 *  Always review for vulnerabilities, logical fallacies, and improper memory read/write operations.
 *  They only resolve the most common pitfalls and are not intended for areas where security is critical.
 *  For example, they will not work with stack-allocated memory and will likely crash the program.
 *  If this applies to your domain or development environment, discard C for a language like Rust.
 */

#ifndef RECSIO_H
#define RECSIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if __STDC_VERSION__ >= 201112l
    #if __has_include("threads.h")
        #include <threads.h>
    #else
        #include "Compat/threads.h"
    #endif
#endif

#if __STDC_VERSION__ < 202311l
    #include <stdbool.h>
    #define nullptr NULL
#endif

#define REC_realloc(_ptr, _len) REC_realloc_mem((void *restrict *const restrict)(&_ptr), _len)
#define REC_free(_ptr) REC_free_mem((void *restrict *const restrict)(&_ptr))
#define REC_probable(_x) __builtin_expect(_x, 1)
#define REC_improbable(_x) __builtin_expect(_x, 0)

////////////////////////////////////////////////
/// @brief  Skips blank characters in a string.
/// @param  _str
////////////////////////////////////////////////
static inline void REC_skipBlank(char *restrict *const restrict _str)
{
    if (_str && *_str)
    {
        while (isblank(**_str))
        {
            (*_str)++;
        }
    }
}

/** stdlib.h **/

//////////////////////////////////////////////////////////////////
/// @brief          `malloc()` with an error message.
/// @param  _SIZE   Amount of bytes to allocate.
/// @param  _MSG    Error message to print.
/// @param  _FATAL  If `true`, abort on failure.
/// @return         Value of `malloc()`, or NULL on zero `_SIZE`.
//////////////////////////////////////////////////////////////////
void *REC_malloc(const size_t _SIZE, const char *const restrict _MSG, const bool _FATAL)
{
    if (_SIZE)
    {
        void *const restrict mem = malloc(_SIZE);

        if (REC_improbable(!mem))
        {
            if (_MSG)
            {
                fprintf(stderr, "\e[1mRECSIO Error: %s\e[0m\n", _MSG);
            }

            if (_FATAL)
            {
                abort();
            }
        }

        return mem;
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////
/// @brief          `calloc()` with an error message.
/// @param  _BLKS   Number of blocks to allocate.
/// @param  _SIZE   Size of each block.
/// @param  _MSG    Error message to print.
/// @param  _FATAL  If `true`, abort on failure.
/// @return         Value of `calloc()`, or NULL on zero `_SIZE`.
//////////////////////////////////////////////////////////////////
void *REC_calloc(const size_t _BLKS, const size_t _SIZE, const char *const restrict _MSG, const bool _FATAL)
{
    if (_SIZE)
    {
        void *const restrict mem = calloc(_BLKS, _SIZE);

        if (REC_improbable(!mem))
        {
            if (_MSG)
            {
                fprintf(stderr, "\e[1mRECSIO Error: %s\e[0m\n", _MSG);
            }

            if (_FATAL)
            {
                abort();
            }
        }

        return mem;
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief          Automatically updates a pointer after `realloc()` moves it.
/// @param  _mem    Unaliased pointer to the memory to reallocate.
/// @param  _SIZE   Requested size in bytes.
/// @return         Return value of `realloc()`.
/// @note           The old pointer is still valid after reallocation fails.
////////////////////////////////////////////////////////////////////////////////
void *REC_realloc_mem(void *restrict *const restrict _mem, const size_t _SIZE)
{
    if (_mem && _SIZE)
    {
        void *const restrict newMem = realloc(*_mem, _SIZE);

        if (newMem)
        {
            if (newMem != *_mem)
            {
                *_mem = newMem;
            }
        }
        else
        {
            fprintf(stderr, "\e[1mRECSIO Warning: realloc() returned NULL pointer; the original pointer is still valid.\e[0m\n");
        }

        return newMem;
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////
/// @brief          Calls `free()` and assigns `_mem` to NULL.
/// @param  _mem    Unaliased pointer to the allocated memory.
///////////////////////////////////////////////////////////////
void REC_free_mem(void *restrict *const restrict _mem)
{
    if (_mem && *_mem)
    {
        free(*_mem);
        *_mem = nullptr;
    }
}

/** stdio.h **/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief      Reintroduces the deprecated and unsafe `gets()` function as `REC_gets()`.
/// @note       Portable, drop-in replacement for POSIX `getline()`, excluding newlines.
/// @attention  Callers are responsible for freeing the returned string to avoid memory leaks.
/// @param _str Pointer to the string to read into, which will be modified in-place.
/// @return     Success: `*_str` points to a null-terminated string; returns number of characters.
///             EOF with zero characters: `*_str` becomes the empty string; returns `-1`.
///             Allocation failure, `malloc()` or `realloc()`: ditto, but returns `-2`.
///             `_str` passed as a null pointer: returns `-3`.
///////////////////////////////////////////////////////////////////////////////////////////////////
long REC_gets(char *restrict *const restrict _str)
{
    if (_str)
    {
        size_t buffCap = 256, buffLen = 0;
        char *const restrict buffer = REC_malloc(buffCap, "REC_gets(): Could not allocate a string buffer.", true);

        if (buffer)
        {
            int charRead;

            while ((charRead = getchar()) != EOF && charRead != '\n')
            {
                if (buffLen + 1 >= buffCap)
                {
                    char *const restrict extBuff = REC_realloc(buffer, (buffCap <<= 1));

                    if (REC_improbable(!extBuff))
                    {
                        REC_free(buffer);
                        *_str = nullptr;

                        return -2;
                    }
                }

                buffer[buffLen++] = charRead;
            }

            char *const restrict finBuff = REC_realloc(buffer, buffLen + 1);

            if (REC_improbable(!finBuff))
            {
                REC_free(buffer);
                *_str = nullptr;

                return -1;
            }

            buffer[buffLen] = '\0';
            *_str = buffer;

            return charRead == EOF ? charRead : (long)(buffLen);
        }
        else
        {
            return -2;
        }
    }

    return -3;
}

/////////////////////////////////////////////////////
/// @brief          `fopen()` with an error message.
/// @param  _NAME   Name of the file.
/// @param  _MODE   Read/write mode.
/// @return         Return value of `fopen()`.
/////////////////////////////////////////////////////
FILE *REC_fopen(const char *const restrict _NAME, const char *const restrict _MODE)
{
    FILE *const restrict STREAM = fopen(_NAME, _MODE);

    if (!STREAM)
    {
        fprintf(stderr, "\e[1mRECSIO Error: Could not open the file \"%s\".\e[0m\n", _NAME);
    }

    return STREAM;
}

//////////////////////////////////////////////////////////
/// @brief          `fclose()` with an error message.
/// @param _stream  Unaliased pointer to the file stream.
/// @return         Return value of `fclose()`.
//////////////////////////////////////////////////////////
int REC_fclose(FILE *const restrict _stream)
{
    const int STATUS = fclose(_stream);

    if (STATUS == EOF)
    {
        fprintf(stderr, "\e[1mRECSIO Error: Could not close the file stream.\e[0m\n");
    }

    return STATUS;
}

/** string.h **/

////////////////////////////////////////////////////////////////////////////
/// @brief      Matches a command prefix or substring from an input string.
/// @param _str Original input string.
/// @param _CMD Command prefix to match.
/// @return     Pointer to the remainder of `_str`, or NULL if not found.
////////////////////////////////////////////////////////////////////////////
char *REC_strcmd(char *restrict _str, const char *const restrict _CMD)
{
    if (_str && _CMD)
    {
        const size_t CMD_LEN = strlen(_CMD);

        REC_skipBlank(&_str);

        if (!strncmp(_str, _CMD, CMD_LEN))
        {
            const char NEXT = _str[CMD_LEN];

            if (!NEXT || isblank(NEXT))
            {
                char *restrict arg = _str + CMD_LEN;

                REC_skipBlank(&arg);

                return arg;
            }
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////
/// @brief      Concatenates a string to the end of another string.
/// @param _dst Pointer to the destination string.
/// @param _SRC Contents of the source string.
/// @return     The destination string, or NULL on invalid arguments.
/// @note       Reallocates the destination to fit the new string.
//////////////////////////////////////////////////////////////////////
char *REC_strcat(char *restrict *const restrict _dst, const char *const restrict _SRC)
{
    if (_dst && *_dst && _SRC)
    {
        const size_t SRC_LEN = strlen(_SRC);

        REC_realloc(*_dst, strlen(*_dst) + SRC_LEN + 1);

        return strncat(*_dst, _SRC, SRC_LEN);
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
/// @brief      Copys the string contents of the source to the destination.
/// @param _dst Pointer to the destination string.
/// @param _SRC Contents of the source string.
/// @return     The destination string, or NULL on invalid arguments.
/// @note       Reallocates the destination to fit the new string.
////////////////////////////////////////////////////////////////////////////
char *REC_strcpy(char *restrict *const restrict _dst, const char *const restrict _SRC)
{
    if (_dst && *_dst && _SRC)
    {
        const size_t SRC_LEN_P1 = strlen(_SRC) + 1;

        REC_realloc(*_dst, SRC_LEN_P1);

        return strncpy(*_dst, _SRC, SRC_LEN_P1);
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////////////
/// @brief      Obtains the first instance of a character in a string.
/// @param _STR String to search.
/// @param _CHR Character to find.
/// @return     Zero-based index, or `-1` if not found.
/// @note       Case-sensitive ('A' != 'a').
///////////////////////////////////////////////////////////////////////
long REC_strchr(char *const restrict _STR, const char _CHR)
{
    if (_STR)
    {
        char *restrict c = strchr(_STR, _CHR);

        return c ? (long)(c - _STR) : -1;
    }

    return -1;
}


/////////////////////////////////////////////////////////////
/// @brief          Case-insensitive full string comparison.
/// @param  _STR_A  First string.
/// @param  _STR_B  Second string.
/// @return         `true` if they match; otherwise `false`.
/// @note           Similar to POSIX's `strcasecmp()`.
/////////////////////////////////////////////////////////////
bool REC_strcasecmp(const char *const restrict _STR_A, const char *const restrict _STR_B)
{
    const unsigned char *restrict strA = (const unsigned char *restrict)(_STR_A);
    const unsigned char *restrict strB = (const unsigned char *restrict)(_STR_B);

    while (*strA && *strB)
    {
        if (toupper(*strA) != toupper(*strB))
        {
            return false;
        }

        strA++;
        strB++;
    }

    REC_skipBlank((char *restrict *const restrict)(&strA));
    REC_skipBlank((char *restrict *const restrict)(&strB));

    return *strA == *strB;
}

////////////////////////////////////////////////////////////////////
/// @brief          Error-handling wrapper for `thrd_create()`.
/// @param  _thr    Unaliased pointer to the new thread.
/// @param  _func   Pointer to the thread function's start address.
/// @param  _arg    Unaliased pointer to the function argument.
/// @param  _MSG    Error message to print.
/// @param  _FATAL  If `true`, abort on failure.
/// @return         Value of `thrd_create()`.
////////////////////////////////////////////////////////////////////
int REC_thrd_create(thrd_t *restrict _thr, thrd_start_t _func, void *const restrict _arg, const char *const restrict _MSG, const bool _FATAL)
{
    if (_thr && _func)
    {
        const int THR_STAT = thrd_create(_thr, _func, _arg);

        if (REC_improbable(THR_STAT != thrd_success))
        {
            if (_MSG)
            {
                fprintf(stderr, "\e[1mRECSIO Error: %s\e[0m\n", _MSG);
            }

            if (_FATAL)
            {
                abort();
            }
        }

        return THR_STAT;
    }

    return thrd_error;
}

//////////////////////////////////////////////////////////////////////
/// @brief          Error-handling wrapper for `thrd_join()`.
/// @param  _thr    Unaliased pointer to the thread to join.
/// @param  _res    Output parameter to store the thread's exit code.
/// @param  _MSG    Error message to print.
/// @param  _FATAL  If `true`, abort on failure.
/// @return         Value of `thrd_join()`.
//////////////////////////////////////////////////////////////////////
int REC_thrd_join(thrd_t _thr, int *_res, const char *const restrict _MSG, const bool _FATAL)
{
    const int JOIN_STAT = thrd_join(_thr, _res);

    if (REC_improbable(JOIN_STAT != thrd_success))
    {
        if (_MSG)
        {
            fprintf(stderr, "\e[1mRECSIO Error: %s\e[0m\n", _MSG);
        }

        if (_FATAL)
        {
            abort();
        }
    }

    return JOIN_STAT;
}

/////////////////////////////////////////////////////////////
/// @brief          Error-handling wrapper for `mtx_init()`.
/// @param  _mtx    Unaliased pointer to the mutex.
/// @param  _TYPE   Mutex type.
/// @param  _MSG    Error message to print.
/// @param  _FATAL  If `true`, abort on failure.
/// @return         Value of `mtx_init()`.
/////////////////////////////////////////////////////////////
int REC_mtx_init(mtx_t *restrict _mtx, const int _TYPE, const char *const restrict _MSG, const bool _FATAL)
{
    if (_mtx)
    {
        const int MTX_STAT = mtx_init(_mtx, _TYPE);

        if (REC_improbable(MTX_STAT != thrd_success))
        {
            if (_MSG)
            {
                fprintf(stderr, "\e[1mRECSIO Error: %s\e[0m\n", _MSG);
            }

            if (_FATAL)
            {
                abort();
            }
        }

        return MTX_STAT;
    }

    return thrd_error;
}

/////////////////////////////////////////////////////////////////
/// @brief          Error-handling wrapper for `cnd_init()`.
/// @param  _cnd    Unaliased pointer to the condition variable.
/// @param  _MSG    Error message to print.
/// @param  _FATAL  If `true`, abort on failure.
/// @return         Value of `cnd_init()`.
/////////////////////////////////////////////////////////////////
int REC_cnd_init(cnd_t *restrict _cnd, const char *const restrict _MSG, const bool _FATAL)
{
    if (_cnd)
    {
        const int CND_STAT = cnd_init(_cnd);

        switch (CND_STAT)
        {
        case thrd_error:
            if (_MSG)
            {
                fprintf(stderr, "\e[1mRECSIO Error: %s\e[0m\n", _MSG);
            }
            if (_FATAL)
            {
                abort();
            }
            break;
        case thrd_nomem:
            fprintf(stderr, "\e[1mRECSIO Error: Insufficient memory to initialize a condition variable.\e[0m\n");
            [[fallthrough]];
        case thrd_success:
            break;
        }

        return CND_STAT;
    }

    return thrd_error;
}

#endif // RECSIO_H //
