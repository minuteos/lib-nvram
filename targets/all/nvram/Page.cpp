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
    return FromPtrInline(ptr);
}

/*!
 * Compares the relative age of two records, according to sequence number of pages on which they are located.
 */
int Page::CompareAge(const void* rec1, const void* rec2)
{
    const Page* p1 = FromPtrInline(rec1);
    const Page* p2 = FromPtrInline(rec2);

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
const Page* Page::UnorderedNextImpl(const Page* after)
{
    return FastEnum(after->Block(), after + 1, after->id);
}

const Page* Page::OldestNextImpl(const Page* after)
{
    return after->OldestNext();
}

const Page* Page::NewestNextImpl(const Page* after)
{
    return after->NewestNext();
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
 * Duplicate sequence handling - the oldest page is the one enumerated first,
 * while the newest one is the one enumerated last
 * Indeterminate sequence handling - the oldest and newest page sequence is
 * always determined relative to the first page
 */
Packed<Page::FirstScanResult> Page::Scan(ID id)
{
    const Page* p = First(id);
    const Page* oldest = p;
    const Page* newest = p;

    if (p)
    {
        // start and end is disambiguated by first page sequence
        uint16_t seqBase = p->sequence;
        while ((p = p->Next()))
        {
            if (OVF_LT(p->sequence, seqBase) && OVF_LT(p->sequence, oldest->sequence))
            {
                oldest = p;
            }
            if (OVF_GE(p->sequence, seqBase) && OVF_GE(p->sequence, newest->sequence))
            {
                newest = p;
            }
        }
    }

    return pack<FirstScanResult>(newest, oldest);
}

/*!
 * Returns pointers to an older and newer page relative to to the provided page
 * Duplicate sequence handling - a page is also considered older if it has
 * the same sequence, but a lower address, or newer if it has the same sequence
 * but a higher address
 * Indeterminate sequence handling - sequence start is disambiguated by the
 * first page with the specified ID
 */
Packed<Page::NextScanResult> Page::Scan(ID id, const Page* relativeTo)
{
    const Page* p = First(id);
    const Page* oldest = p;
    const Page* newest = p;
    const Page* older = NULL;
    const Page* newer = NULL;

    if (p)
    {
        // start and end is disambiguated by first page sequence
        uint16_t seqBase = p->sequence;
        do
        {
            if (OVF_LT(p->sequence, seqBase) && OVF_LT(p->sequence, oldest->sequence))
            {
                oldest = p;
            }
            if (OVF_GE(p->sequence, seqBase) && OVF_GE(p->sequence, newest->sequence))
            {
                newest = p;
            }
            if ((p->sequence == relativeTo->sequence && p < relativeTo) ||
                (OVF_LT(p->sequence, relativeTo->sequence) && (!older || OVF_GE(p->sequence, older->sequence))))
            {
                older = p;
            }
            if (((p->sequence == relativeTo->sequence && p > relativeTo) || OVF_GT(p->sequence, relativeTo->sequence)) &&
                (!newer || OVF_LT(p->sequence, newer->sequence)))
            {
                newer = p;
            }
        }
        while ((p = p->Next()));

        // discard the found pages if the sequence starts a new loop
        if (older == newest)
        {
            older = NULL;
        }
        if (newer == oldest)
        {
            newer = NULL;
        }
    }

    return pack<NextScanResult>(older, newer);
}

}
