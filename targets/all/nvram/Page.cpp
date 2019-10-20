/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Page.cpp
 */

#include <nvram/Page.h>
#include <nvram/Block.h>
#include <nvram/Manager.h>

#define MYDBG(...)  DBGCL("nvram", __VA_ARGS__)

namespace nvram
{

/*!
 * Returns pointer to the page which contains ptr
 */
const Page* Page::FromPtr(const void* ptr)
{
    uintptr_t firstPageInBlock = ((uintptr_t)ptr & BlockMask) + BlockHeader;
    ASSERT(firstPageInBlock > (uintptr_t)_manager.Blocks().begin() && firstPageInBlock < (uintptr_t)_manager.Blocks().end());
    return (const Page*)((uintptr_t)ptr - ((uintptr_t)ptr - firstPageInBlock) % PageSize);
}

/*!
 * Compares the relative age of two records, according to sequence number of pages on which they are located.
 */
int Page::CompareAge(const void* rec1, const void* rec2)
{
    const Page* p1 = FromPtr(rec1);
    const Page* p2 = FromPtr(rec2);

    if (p1 != p2)
        return OVF_DIFF(p1->sequence, p2->sequence);

    // within the same page, the older records have lower addresses
    return (uintptr_t)rec1 - (uintptr_t)rec2;
}

/*!
 * Checks if a page is completely empty, that is contains all ones
 */
bool Page::CheckEmpty() const
{
    const uint32_t* p = (const uint32_t*)this;
    const uint32_t* e = (const uint32_t*)(this + 1);
    while (p != e)
    {
        if (*p++ != ~0u)
            return false;
    }

    return true;
}

/*!
 * Returns the first existing page with the specified ID, or NULL if such a page doesn't exist
 *
 * The page returned is not guaranteed to be of any specific age (i.e. oldest or newest)
 */
const Page* Page::First(ID id)
{
    auto* blk = _manager.UsedBlocks().begin();
    return blk == _manager.UsedBlocks().end() ? NULL : FastEnum(blk, blk->begin(), id);
}

/*!
 * Returns the next page with the specified ID, or NULL if no next page was found
 *
 * The pages are returned in an unspecified order
 */
const Page* Page::Next() const
{
    return FastEnum(Block(), this + 1, id);
}

/*!
 * Continues enumeration of pages started by Page::First(), in an unspecified order.
 */
const Page* Page::FastEnum(const nvram::Block* blk, const Page* p, ID id)
{
    // first finish searching the current block
    for (;;)
    {
        auto end = blk->end();
        for (; p != end; p++)
        {
            if (p->id == ~0u)
                break;
            if (p->id == id)
                return p;
        }

        for (;;)
        {
            blk++;
            if (blk == _manager.UsedBlocks().end())
                return NULL;
            if (blk->IsValid())
                break;
        }
        p = blk->begin();
    }
}

/*!
 * Returns packed pointers to the oldest (R1) and newest (R0) page with the specified ID.
 */
res_pair_t Page::Scan(ID id)
{
    const Page* p = First(id);
    const Page* oldest = p;
    const Page* newest = p;

    if (p)
    {
        while ((p = p->Next()))
        {
            if (OVF_GT(oldest->sequence, p->sequence))
                oldest = p;
            if (OVF_LT(newest->sequence, p->sequence))
                newest = p;
        }
    }

    return RES_PAIR(newest, oldest);
}

/*!
 * Returns pointers to an older and newer page relative to to the provided sequence number.
 */
res_pair_t Page::Scan(ID id, uint16_t seq)
{
    const Page* older = NULL;
    const Page* newer = NULL;

    for (auto* p = First(id); p; p = p->Next())
    {
        if (OVF_LT(p->sequence, seq) && (!older || OVF_GT(p->sequence, older->sequence)))
            older = p;
        if (OVF_GT(p->sequence, seq) && (!newer || OVF_LT(p->sequence, newer->sequence)))
            newer = p;
    }

    return RES_PAIR(older, newer);
}

}
