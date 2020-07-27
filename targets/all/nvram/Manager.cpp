/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Manager.cpp
 */

#include <nvram/nvram.h>

#if Tcortex_m
#include <ld_symbols.h>
#endif

#define MYDBG(...)  DBGCL("nvram", __VA_ARGS__)

namespace nvram
{

Manager _manager;

/*!
 * Scans the area reserved for NVRAM for existing Blocks,
 * fixes any problems that may have been created by an unexpected reset,
 * so that we can make some assumptions about the flash layout
 * and not bother with them when doing scans in the future
 *
 * All blocks can be assumed to be in one of three states after initialization is complete,
 * indicated by the first word
 * - valid (magic value)
 * - free (all ones)
 * - erasable (zero) - note that generation field should be still valid or zero
 */
bool Manager::Initialize(Span area, InitFlags flags)
{
    if (!area)
    {
        // calculate NVRAM area, leaving areas at the beginning and end of flash for other uses
        area = Flash::GetRange();
        UNUSED auto size = area.Length();
#ifdef NVRAM_RESERVED_START_FRACTION
        area = area.RemoveLeft(size * NVRAM_RESERVED_START_FRACTION);
#elif defined(NVRAM_RESERVED_START)
        area = area.RemoveLeft(NVRAM_RESERVED_START);
#endif
#ifdef NVRAM_RESERVED_END_FRACTION
        area = area.RemoveRight(size * NVRAM_RESERVED_END_FRACTION);
#elif defined(NVRAM_RESERVED_END)
        area = area.RemoveRight(NVRAM_RESERVED_END);
#endif
    }

    // align to actual block boundaries
    blkStart = (const Block*)(((uintptr_t)area.begin() + ~BlockMask) & BlockMask);
    blkFirst = blkEnd = (const Block*)((uintptr_t)area.end() & BlockMask);
    pagesAvailable = 0;
    collectors.Clear();
    notifiers.Clear();
    collecting = false;
    int corrupted = 0;

    ASSERT(blkStart < blkEnd);

#if TRACE && Tcortex_m
    if ((const char*)blkStart < &__data_load_end)
    {
        MYDBG("ERROR: image overlaps NVRAM area (%p > %p) - modify NVRAM_RESERVED_START(_FRACTION)", &__data_load_end, blkStart);
        ASSERT(false);
    }
#endif

    if (!!(flags & InitFlags::Reset))
    {
        MYDBG("Erasing NVRAM area %p - %p", area.begin(), area.end());
        Flash::Erase(area);
    }

    for (auto* blk = blkEnd - 1; blk >= blkStart; blk--)
    {
        if (blk->magic == Block::Magic)
        {
            blkFirst = blk;

            if (blk->generation == ~0u)
            {
                // appears to be a half-initialized sector
                MYDBG("Found half-initialized sector (magic but no generation) @ %08X", blk);
                if (blk->CheckEmpty(&blk->generation + 1))
                {
                    // try to initialize
                    if (blk->Format(1))
                    {
                        // success
                        continue;
                    }
                }

                // mark it for erasure
                MYDBG("ERROR - Failed to complete block initialization @ %08X", blk);
                Flash::ShredWord(&blk->generation);
                Flash::ShredWord(&blk->magic);
            }
            else
            {
                // scan through pages to see if the block can be erased
                auto res = blk->CheckPages();
                if (RES_PAIR_FIRST(res) == Block::PagesErasable)
                {
                    // mark a block with only erasable pages as erasable
                    MYDBG("WARNING - Block with no used nor free pages found after reset @ %08X", blk);
                    Flash::ShredWord(&blk->magic);
                    blocksToErase = true;
                }
                else
                {
                    pagesAvailable += RES_PAIR_SECOND(res);
                }
            }
        }
        else if (blk->CheckEmpty())
        {
            // verified free block, add to free page pool
            pagesAvailable += PagesPerBlock;
        }
        else if (blk->IsErasable())
        {
            // block is already scheduled for erase
            MYDBG("WARNING - Block marked for erase found after reset @ %08X", blk);
            blocksToErase = true;
        }
        else if (!!(flags & InitFlags::IgnoreCorrupted))
        {
            corrupted++;
        }
        else
        {
            // unless marked erasable, there is something wrong with the block (e.g. interrupted erase operation)
            MYDBG("WARNING - Corrupted block @ %08X", blk);
            Flash::ShredWord(&blk->generation);
            Flash::ShredWord(&blk->magic);
            blocksToErase = true;
        }
    }

    MYDBG("Init complete - %08X <= %08X <= %08X", blkStart, blkFirst, blkEnd);
    MYDBG("  %d/%d pages free (%d/%d bytes)", pagesAvailable, PagesPerBlock * (blkEnd - blkStart - corrupted),
        pagesAvailable * PagePayload, (PagesPerBlock * (blkEnd - blkStart - corrupted)) * PagePayload);

    if (corrupted)
    {
        MYDBG("  %d corrupted pages left unerased", corrupted);
        return false;
    }

    if (blocksToErase)
    {
        MYDBG("There are blocks marked to be erased, running collector");
        RunCollector();
    }
    else if (pagesAvailable < PagesKeptFree)
    {
        MYDBG("Not enough pages free, running collector");
        RunCollector();
    }

    return true;
}

const Block* Manager::NewBlock()
{
    // we can use only empty blocks, erase is too time consuming to perform synchronously
    for (auto* blk = blkEnd - 1; blk >= blkStart; blk--)
    {
        if (blk->IsEmpty() && blk->Format(1))
        {
            if (blkFirst > blk)
                blkFirst = blk;
            return blk;
        }
    }

    return NULL;
}

/*!
 * Allocates a new page with the specified ID
 */
const Page* Manager::NewPage(ID id, uint32_t recordSize)
{
    uint32_t seq = ~0u;
    const Page* free = NULL;

    // find all required information (new sequence and placement) in a single pass
    for (auto& b: Blocks(blkFirst))
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
            else if (!free && p.IsEmpty())
            {
                free = &p;
                break;
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
            if (auto* pb = NewBlock())
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
                MYDBG("Allocated page %.4s-%d with fixed record size %u @ %08X", &id, seq, recordSize, free);
            }
            else
            {
                MYDBG("Allocated page %.4s-%d with variable record size @ %08X", &id, seq, free);
            }

            pagesAvailable--;

            // always run the collector after allocating a new page
            RunCollector();

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
            for (auto& b : Blocks(Block::FromPtr(free) + 1))
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

void Manager::RegisterCollector(ID key, unsigned level, CollectorDelegate delegate)
{
    auto m = collectors.Manipulate();

    while (m)
    {
        if (m.Element().key == key && m.Element().level == level)
        {
            // replace existing registration
            m.Element().collector = delegate;
            return;
        }
        else if (m.Element().level > level)
        {
            // insert here
            break;
        }

        ++m;
    }

    m.Insert({ key, level, delegate });
}

void Manager::RunCollector()
{
    if (!collecting)
    {
        collecting = true;
        MYDBG("Scheduling collector task");
        kernel::Task::Run(this, &Manager::Collector);
    }
}

async(Manager::Collector)
async_def()
{
    MYDBG("Collection starting with %d pages free", pagesAvailable);

    // always run a non-destructive collection
    Collect(false);

    for (;;)
    {
        if (blocksToErase)
        {
           await(EraseBlocks);
        }

        if (pagesAvailable >= PagesKeptFree)
        {
            MYDBG("Collection finished with %d pages free", pagesAvailable);
            break;
        }

        async_yield();

        if (!Collect(true) && !blocksToErase)
        {
            MYDBG("Collection finished with only %d pages free", pagesAvailable);
            break;
        }
    }

    collecting = false;
}
async_end

int Manager::Collect(bool destructive)
{
    int collected = 0;

    for (auto& collector: collectors)
    {
        if (!destructive && collector.level)
        {
            // only non-destructive collectors (level 0) are executed
            break;
        }

        if (auto page = collector.collector(collector.key))
        {
            MYDBG("Page %.4s-%d @ %08X can be erased", &page->id, page->sequence, page);
            Flash::ShredWord(&page->id);
            collected++;
            // mark the entire block erasable if it contains only erasable pages
            auto* b = page->Block();
            if (RES_PAIR_FIRST(page->Block()->CheckPages()) == Block::PagesErasable)
            {
                Flash::ShredWord(&b->magic);
                blocksToErase = true;
            }

            if (collector.level)
            {
                // do not run any more collectors above level 0 at once
                break;
            }
        }
    }

    return collected;
}

async(Manager::EraseBlocks)
async_def(const Block* block; uint32_t gen)
{
    for (f.block = blkStart; f.block != blkEnd; f.block++)
    {
        if (f.block->IsErasable())
        {
            f.gen = f.block->generation;

            for (;;)
            {
                MYDBG("Trying to erase block @ %08X", f.block);
                if (await(Flash::ErasePageAsync, f.block))
                {
                    break;
                }
                MYDBG("Erase of block interrupted @ %08X", f.block);
            }

            if (!f.block->CheckEmpty())
            {
                MYDBG("ERROR - block not completely erased @ %08X");
            }
            else if (!f.gen || f.block->Format(f.gen + 1))  // no reason to format gen 1 blocks
            {
                pagesAvailable += PagesPerBlock;
                continue;
            }

            // something has gone wrong, mark block for another erasure attempt
            Flash::ShredWord(&f.block->generation);
            Flash::ShredWord(&f.block->magic);
        }
    }

    // do not attempt another erase even if some blocks failed
    // to avoid trying in an infinite loop
    blocksToErase = false;
}
async_end

const Page* CollectorDiscardOldest(void* arg0, ID id)
{
    return Page::OldestFirst(id);
}

const Page* CollectorRelocate(void* arg0, ID id)
{
    const Page* newest;
    const Page* oldest;

    Page::Scan(id, oldest, newest);
    if (oldest == newest)
    {
        // less than two pages, no way to relocate anything
        return NULL;
    }

    for (; oldest != newest; oldest = oldest->OldestNext())
    {
        // try to move records - never move more than half of a page's
        // usable payload to avoid moving that actually just
        // copy the entire old page to the new one
        if (oldest->MoveRecords(newest, PagePayload / 2))
        {
            return oldest;
        }
    }

    return NULL;
}

static void IncrementVersion(unsigned* pVersion, ID pageType)
{
    (*pVersion)++;
}

void Manager::RegisterVersionTracker(ID pageType, unsigned* pVersion)
{
    *pVersion = 1;
    RegisterNotifier(pageType, GetDelegate(&IncrementVersion, pVersion));
}

void Manager::Notify(ID id)
{
    for (auto& notifier : notifiers)
    {
        if (notifier.key == id)
        {
            notifier.notifier(id);
        }
    }
}

size_t Manager::EraseAll(ID id)
{
    size_t count = 0;
    for (auto page = Page::First(id); page; page = Page::FastEnum(page->Block(), page + 1, id))
    {
        Flash::ShredWord(page);
        count++;
    }

    if (count)
    {
        RunCollector();
    }

    return count;
}

}
