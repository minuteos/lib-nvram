/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Page.cpp
 */

#include <nvram/Page.h>
#include <nvram/Block.h>

#define MYDBG(...)  DBGCL("nvram", __VA_ARGS__)

namespace nvram
{

/*!
 * Returns pointer to the page which contains ptr
 */
const Page* Page::FromPtr(const void* ptr)
{
    uintptr_t firstPageInBlock = ((uintptr_t)ptr & BlockMask) + BlockHeader;
    ASSERT(firstPageInBlock > (uintptr_t)Block::s_start && firstPageInBlock < (uintptr_t)Block::s_end);
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
 * Allocates a new page with the specified ID
 */
const Page* Page::New(ID id, uint32_t recordSize)
{
    uint32_t seq = ~0u;
    const Page* free = NULL;

    // find all required information (new sequence and placement) in a single pass
    for (auto& b: Block::Enumerate())
    {
        if (!b.IsValid())
            continue;

        for (auto& p: b)
        {
            if (p.id == id)
            {
                if (seq == ~0u || OVF_LE((uint16_t)seq, p.sequence))
                    seq = p.sequence + 1;
            }
            else if (!free && p.id == ~0u)
            {
                if (!p.IsEmpty())
                {
                    // mark the page as bad
                    MYDBG("Marking corrupted page @ %08X", p);
                    Flash::ShredWord(&p);
                }
                else
                {
                    free = &p;
                    break;
                }
            }
        }
    }

    if (seq == ~0u)
        seq = 1;

    uint32_t w0 = (seq & MASK(16)) | (recordSize << 16);
    for (;;)
    {
        if (!free)
        {
            // try to format a new block if possible
            if (auto* pb = Block::New())
            {
                free = pb->begin();
            }
            else
            {
                // cannot allocate now
                break;
            }
        }

        // try to prepare a page
        if (Flash::WriteWord(&free->sequence, w0) &&
            Flash::WriteWord((const uint32_t*)&free->id, id))
        {
            if (recordSize)
            {
                MYDBG("Allocated page %.4s-%d with fixed record size %u @ %08X\n", &id, seq, recordSize, free);
            }
            else
            {
                MYDBG("Allocated page %.4s-%d with variable record size @ %08X\n", &id, seq, free);
            }

            return free;	// page is ready
        }

        // mark the page as bad
        Flash::ShredWord(free);
        MYDBG("ERROR - Failed to format page %.4s-%d @ %08X", &id, seq, free);

        for (free++; free != free->Block()->end(); free++)
        {
            if (free->IsEmpty())
                break;

            if (free->id == ~0u)
            {
                MYDBG("Marking corrupted page @ %08X", free);
                Flash::ShredWord(free);
            }
        }

        if (free == free->Block()->end())
        {
            // this block is full, we need a new one
            free = NULL;
            for (auto& b : Block::Enumerate(Block::FromPtr(free) + 1))
            {
                if (!b.IsValid())
                    continue;

                for (auto& p: b)
                {
                    if (p.id == ~0u)
                    {
                        if (!p.IsEmpty())
                        {
                            // mark the page as bad
                            MYDBG("Marking corrupted page @ %08X", p);
                            Flash::ShredWord(&p);
                        }
                        else
                        {
                            free = &p;
                            break;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}

/*!
 * Checks if a page is completely empty, that is contains all ones
 */
bool Page::IsEmpty() const
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
    auto* blk = Block::s_first;
    return blk == Block::s_end ? NULL : FastEnum(blk, blk->begin(), id);
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
            if (blk == Block::s_end)
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
