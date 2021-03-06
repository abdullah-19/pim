#include "containers/ptrqueue.h"
#include "common/atomics.h"
#include "common/nextpow2.h"
#include "allocator/allocator.h"

void ptrqueue_create(ptrqueue_t* pq, EAlloc allocator, u32 capacity)
{
    ASSERT(pq);
    pq->iWrite = 0;
    pq->iRead = 0;
    pq->width = 0;
    pq->ptr = NULL;
    pq->allocator = allocator;
    ASSERT(capacity);

    const u32 width = NextPow2(capacity);
    pq->width = width;
    pq->ptr = pim_calloc(allocator, sizeof(pq->ptr[0]) * width);
}

void ptrqueue_destroy(ptrqueue_t* pq)
{
    pim_free(pq->ptr);
    pq->ptr = 0;
    store_u32(&(pq->width), 0, MO_Release);
    store_u32(&(pq->iWrite), 0, MO_Release);
    store_u32(&(pq->iRead), 0, MO_Release);
}

u32 ptrqueue_capacity(const ptrqueue_t* pq)
{
    ASSERT(pq);
    return load_u32(&(pq->width), MO_Acquire);
}

u32 ptrqueue_size(const ptrqueue_t* pq)
{
    ASSERT(pq);
    return load_u32(&(pq->iWrite), MO_Acquire) - load_u32(&(pq->iRead), MO_Acquire);
}

bool ptrqueue_trypush(ptrqueue_t* pq, void* pValue)
{
    ASSERT(pq);
    ASSERT(pValue);
    const isize iPtr = (isize)pValue;
    const u32 mask = pq->width - 1u;
    isize* ptr = pq->ptr;
    for (u32 i = load_u32(&(pq->iWrite), MO_Acquire); ptrqueue_size(pq) <= mask; ++i)
    {
        i &= mask;
        isize prev = load_isize(ptr + i, MO_Relaxed);
        if (!prev && cmpex_isize(ptr + i, &prev, iPtr, MO_Acquire))
        {
            inc_u32(&(pq->iWrite), MO_Release);
            return true;
        }
    }
    return false;
}

void* ptrqueue_trypop(ptrqueue_t* pq)
{
    ASSERT(pq);
    const u32 mask = pq->width - 1u;
    isize* ptr = pq->ptr;
    for (u32 i = load_u32(&(pq->iRead), MO_Acquire); ptrqueue_size(pq); ++i)
    {
        i &= mask;
        isize prev = load_isize(ptr + i, MO_Relaxed);
        if (prev && cmpex_isize(ptr + i, &prev, 0, MO_Acquire))
        {
            inc_u32(&(pq->iRead), MO_Release);
            ASSERT(prev);
            return (void*)prev;
        }
    }
    return NULL;
}
