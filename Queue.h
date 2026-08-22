/*
 *  Author: 2025- TheTrustedComputer
 *  
 *  A queue is a FIFO data structure for storing elements; it can accept arbitrary data types.
 *  It is implemented as a singly linked list to achieve constant-time enqueuing and dequeuing.
 *  A naive method would only store the head, but doing the same for the tail is standard practice.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if __STDC_VERSION__ < 202311l
    #include <stdbool.h>
    #define nullptr NULL
#endif

#ifndef RECSIO_H
    #include "RECSIO.h"
#endif

#define Queue_probable(_x) __builtin_expect(_x, 1)

#pragma pack(push, 1)

typedef struct QueueNode
{
    struct QueueNode *restrict next;
    void *restrict data;
}
QueueNode;

typedef struct
{
    QueueNode *restrict head, *restrict tail;
}
QueueList;

#pragma pack(pop)

////////////////////////////////////////////////////
/// @brief      Tests if the queue has no elements.
/// @param _Q   Unaliased pointer to the queue.
/// @return     `true` if it is; otherwise `false`.
////////////////////////////////////////////////////
bool QueueList_empty(const QueueList *const restrict _Q)
{
    return !(_Q->head || _Q->tail);
}

////////////////////////////////////////////////////////
/// @brief      Initializes a queue with null pointers.
/// @param _q   Unaliased pointer to the queue.
////////////////////////////////////////////////////////
void QueueList_init(QueueList *const restrict _q)
{
    memset(_q, 0, sizeof(*_q));
}

///////////////////////////////////////////////////
/// @brief      Releases memory used by the queue.
/// @param _q   Unaliased pointer to the queue.
///////////////////////////////////////////////////
void QueueList_destroy(QueueList *const restrict _q)
{
    for (QueueNode *restrict *const restrict delQ = &_q->head, *restrict delQn; _q->head; *delQ = delQn)
    {
        delQn = (*delQ)->next;
        REC_free(*delQ);
    }
    
    _q->tail = nullptr;
}

////////////////////////////////////////////////////////////
/// @brief          Enqueues data to the tail of the queue.
/// @param _q       Unaliased pointer to the queue.
/// @param _data    Pointer to the raw data to append.
////////////////////////////////////////////////////////////
void QueueList_push(QueueList *const restrict _q, void *const restrict _data)
{
    if (_q->head)
    {
        QueueNode *const restrict addQ = REC_calloc(1, sizeof(*addQ), "Could not allocate memory to enqueue data.", true);
        addQ->data = _data;
        _q->tail->next = addQ;
        _q->tail = addQ;
    }
    else
    {
        _q->head = REC_calloc(1, sizeof(*_q->head), "Could not allocate memory for the queue head.", true);
        _q->head->data = _data;
        _q->tail = _q->head;
    }
}

//////////////////////////////////////////////////////////
/// @brief      Dequeues data from the head of the queue.
/// @param _q   Unaliased pointer to the queue.
//////////////////////////////////////////////////////////
void QueueList_pop(QueueList *const restrict _q)
{
    if (Queue_probable(_q->head != _q->tail))
    {
        QueueNode *const restrict remQ = _q->head;
        _q->head = _q->head->next;
        REC_free(remQ);
    }
    else
    {
        REC_free(_q->head);
        _q->tail = nullptr;
    }
}

#endif // QUEUE_H //
