/*
 *  Author: 2025- TheTrustedComputer
 *  
 *  Memory pools are an optimization technique for efficient allocation and deallocation without the overhead of malloc() and free().
 *  By using large preallocated blocks and growing them as needed, memory pools can reduce fragmentation and improve cache performance.
 *  We implemented a rudimentary bump allocator that prioritizes user data over metadata, moving the free list pointers as necessary.
 */

#ifndef BUMPPOOL_H
#define BUMPPOOL_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#if __STDC_VERSION__ >= 201112l
    #if __has_include("threads.h")
        #include <threads.h>
#else
        #include "Compat/threads.h"
    #endif
#endif

#if __STDC_VERSION__ < 202311l
    #define constexpr const
    #define nullptr NULL
#endif

#ifndef RECSIO_H
    #include "RECSIO.h"
#endif

#define MemoryPool_probable(_x) __builtin_expect(_x, 1)
#define MemoryPool_improbable(_x) __builtin_expect(_x, 0)

#pragma pack(push, 1)

typedef struct MemoryBlock
{
    struct MemoryBlock *restrict next;  // Pointer to next free block
    size_t size;                        // Free memory size, excluding header
}
MemoryBlock;

typedef struct
{
    void *restrict *restrict baseAddr;  // Array of base addresses
    MemoryBlock *restrict freeList;     // Pointer to first free block
    MemoryBlock *restrict lastFree;     // Pointer to last free block
    size_t size, chunks;                // Managed size; number of allocated chunks
    mtx_t lock;                         // Mutex for thread safety
}
MemoryPool;

#pragma pack(pop)

static constexpr size_t MEMPOOL_INIT_SIZE = 1073741824; // 1 GB
static constexpr size_t MEMPOOL_EXPAND_RATE = MEMPOOL_INIT_SIZE;
static constexpr size_t MEMPOOL_HEADER_SIZE = sizeof(MemoryBlock);

////////////////////////////////////////////////////////////////
/// @brief          Finds a free block with a given size.
/// @param _pool    Unaliased pointer to the memory pool.
/// @param _SIZE    Requested block size in bytes.
/// @param _pFree   Output pointer for the previous free block.
/// @param _cFree   Output pointer for the current free block.
////////////////////////////////////////////////////////////////
static inline void MemoryPool_findFree(MemoryPool *const restrict _pool, const size_t _SIZE, MemoryBlock *restrict *const restrict _pFree, MemoryBlock *restrict *const restrict _cFree)
{
    *_pFree = nullptr;
    *_cFree = _pool->freeList;
    
    while (*_cFree)
    {
        if (MemoryPool_probable((*_cFree)->size >= _SIZE))
        {
            return;
        }
        
        *_pFree = *_cFree;
        *_cFree = (*_cFree)->next;
    }
    
    *_pFree = nullptr;
}

////////////////////////////////////////////////////////////////
/// @brief          Initializes a memory pool with zero blocks.
/// @param _pool    Unaliased pointer to the memory pool.
/// @return         Zero if successful; otherwise non-zero.
////////////////////////////////////////////////////////////////
int MemoryPool_init(MemoryPool *const restrict _pool)
{
    _pool->size = MEMPOOL_INIT_SIZE;
    _pool->chunks = 0;
    
    if (MemoryPool_improbable(!(_pool->baseAddr = REC_malloc(sizeof(*_pool->baseAddr) * (_pool->chunks + 1), "Could not initialize the base address array.", false))))
    {
        return 1;
    }
    
    if (MemoryPool_improbable(!(_pool->baseAddr[_pool->chunks] = REC_malloc(_pool->size, "Could not initialize the memory pool.", false))))
    {
        return 2;
    }
    
    if (MemoryPool_improbable(_pool->size < MEMPOOL_HEADER_SIZE))
    {
        fprintf(stderr, "\e[1mMEMORY POOL ERROR: Block size is too small.\e[0m\n");
        REC_free(_pool->baseAddr[_pool->chunks]);
        REC_free(_pool->baseAddr);
        
        return 3;
    }
    
    if (MemoryPool_improbable(REC_mtx_init(&_pool->lock, mtx_plain, "Could not initialize the memory pool mutex.", false) != thrd_success))
    {
        REC_free(_pool->baseAddr[_pool->chunks]);
        REC_free(_pool->baseAddr);
        
        return 4;
    }
    
    _pool->freeList = _pool->baseAddr[_pool->chunks];
    _pool->freeList->size = _pool->size - MEMPOOL_HEADER_SIZE;
    _pool->freeList->next = nullptr;
    _pool->lastFree = _pool->freeList;
    
    return 0;
}

///////////////////////////////////////////////////////////////
/// @brief          Releases all memory used by a memory pool.
/// @param _pool    Unaliased pointer to the memory pool.
///////////////////////////////////////////////////////////////
void MemoryPool_destroy(MemoryPool *const restrict _pool)
{
    if (_pool->baseAddr)
    {
        for (size_t i = 0; i <= _pool->chunks; i++)
        {
            REC_free(_pool->baseAddr[i]);
        }
        
        REC_free(_pool->baseAddr);
        _pool->freeList = _pool->lastFree = nullptr;
    }
    
    _pool->size = _pool->chunks = 0;
    mtx_destroy(&_pool->lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief          Expands the memory pool by a factor of MEMPOOL_EXPAND_RATE.
/// @param _pool    Unaliased pointer to the memory pool.
/// @return         Zero if successful, otherwise non-zero.
///                 1: Expansion failed.
///                 2: Reallocation failed.
////////////////////////////////////////////////////////////////////////////////
int MemoryPool_expand(MemoryPool *const restrict _pool)
{
    void *const restrict expandChunk = REC_malloc(MEMPOOL_EXPAND_RATE, "Could not expand the memory pool, not increasing size.", false);
    
    if (MemoryPool_improbable(!expandChunk))
    {
        return 1;
    }
    
    void *restrict *const restrict expandBase = REC_realloc(_pool->baseAddr, sizeof(*_pool->baseAddr) * (++_pool->chunks + 1));
    
    if (MemoryPool_improbable(!expandBase))
    {
        REC_free(expandChunk);
        
        return 2;
    }
    
    _pool->baseAddr = expandBase;
    _pool->baseAddr[_pool->chunks] = expandChunk;
    
    MemoryBlock *const restrict chunkBlock = expandChunk; 
    
    chunkBlock->size = MEMPOOL_EXPAND_RATE - MEMPOOL_HEADER_SIZE;
    chunkBlock->next = nullptr;
    
    _pool->lastFree ? (_pool->lastFree->next = chunkBlock) : (_pool->freeList = chunkBlock);
    _pool->lastFree = chunkBlock;
    _pool->size += MEMPOOL_EXPAND_RATE;
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////
/// @brief          Allocates a block within a memory pool of a given size.
/// @param _pool    Unaliased pointer to the memory pool.
/// @param _SIZE    Requested block size in bytes.
/// @return         Base address to the found free block; otherwise NULL.
////////////////////////////////////////////////////////////////////////////
void *MemoryPool_alloc(MemoryPool *const restrict _pool, const size_t _SIZE)
{
    if (_SIZE)
    {
        MemoryBlock *restrict prevBlk, *restrict currBlk;
        
        for (;;)
        {
            MemoryPool_findFree(_pool, _SIZE, &prevBlk, &currBlk);
            
            if (MemoryPool_improbable(!currBlk))
            {
                if (MemoryPool_improbable(MemoryPool_expand(_pool)))
                {
                    return nullptr;
                }
                
                continue;
            }
            
            break;
        }
        
        const size_t REM = currBlk->size - _SIZE;
        
        if (MemoryPool_probable(REM >= MEMPOOL_HEADER_SIZE))
        {
            MemoryBlock *const restrict splitBlk = (MemoryBlock*)((uint8_t*)(currBlk) + _SIZE);
            
            splitBlk->size = REM;
            splitBlk->next = currBlk->next;
            prevBlk ? (prevBlk->next = splitBlk) : (_pool->freeList = splitBlk);
            _pool->lastFree == currBlk ? (_pool->lastFree = splitBlk) : 0;
        }
        else
        {
            prevBlk ? (prevBlk->next = currBlk->next) : (_pool->freeList = currBlk->next);
            _pool->lastFree == currBlk ? (_pool->lastFree = prevBlk) : 0;
        }
        
        return currBlk;
    }
    
    return nullptr;
}

////////////////////////////////////////////////////////////
/// @brief          Zeros all values used by a memory pool.
/// @param _pool    Unaliased pointer to the memory pool.
////////////////////////////////////////////////////////////
void MemoryPool_clear(MemoryPool *const restrict _pool)
{
    for (size_t i = 0; i <= _pool->chunks; i++)
    {
        memset(_pool->baseAddr[i] + MEMPOOL_HEADER_SIZE, 0, _pool->size - MEMPOOL_HEADER_SIZE);
    }
}

/////////////////////////////////////////////////////////////////
/// @brief          Thread-safe version of `MemoryPool_alloc()`.
/// @param _pool    Unaliased pointer to the memory pool.
/// @param _SIZE    Requested block size in bytes.
/////////////////////////////////////////////////////////////////
void *MemoryPool_alloc_safe(MemoryPool *restrict _pool, const size_t _SIZE)
{
    mtx_lock(&_pool->lock);
    void *const restrict allocRes = MemoryPool_alloc(_pool, _SIZE);
    mtx_unlock(&_pool->lock);
    
    return allocRes;
}

//////////////////////////////////////////////////////////////////
/// @brief          Thread-safe version of `MemoryPool_expand()`.
/// @param _pool    Unaliased pointer to the memory pool.
//////////////////////////////////////////////////////////////////
int MemoryPool_expand_safe(MemoryPool *restrict _pool)
{
    mtx_lock(&_pool->lock);
    int expandRes = MemoryPool_expand(_pool);
    mtx_unlock(&_pool->lock);
    
    return expandRes;
}

#endif // BUMPPOOL_H //
