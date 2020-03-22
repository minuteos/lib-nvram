/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Manager.h
 */

#pragma once

#include <kernel/kernel.h>

#include <collections/ArrayIterator.h>
#include <collections/LinkedList.h>

namespace nvram
{

class Page;
class Block;

using CollectorDelegate = Delegate<const Page*, ID>;
using NotifierDelegate = Delegate<void, ID>;

class Manager
{
private:
    struct PageCollector
    {
        ID key;
        unsigned level;
        CollectorDelegate collector;
    };

    struct PageNotifier
    {
        ID key;
        NotifierDelegate notifier;
    };

    //! Start of the area reserved for NVRAM
    const Block* blkStart;
    //! End of the area reserved for NVRAM (not a valid block)
    const Block* blkEnd;
    //! First block containing any data
    const Block* blkFirst;
    //! Count of pages available for allocation
    unsigned pagesAvailable;
    //! If the collector is currently running
    bool collecting;
    //! If there are blocks to be erased
    bool blocksToErase;
    //! List of collectors for various page types
    LinkedList<PageCollector> collectors;
    //! List of notifiers for various page types
    LinkedList<PageNotifier> notifiers;

public:
    //! Sets up the area reserved for NVRAM
    void Initialize(Span area, bool erase);
    //! Registers a collector with the specified key (usually page type), at the specified level
    void RegisterCollector(ID key, unsigned level, CollectorDelegate collector);
    //! Registers a change notifier for the specified page type
    void RegisterNotifier(ID type, NotifierDelegate notifier) { notifiers.Push({ type, notifier }); }
    //! Registers a version number trackker for the specified page type
    void RegisterVersionTracker(ID type, unsigned* pVersion);
    //! Runs the collector process if it is not already running
    void RunCollector();
    //! Notifies that the contents of the specified page type have changed
    void Notify(ID id);

    //! Iterates over all the blocks starting at the specified @ref Block
    //! Invalid blocks in between are returned, make sure to use @ref IsValid before accessing the contents
    constexpr ArrayIterator<const Block> Blocks(const Block* beginAt) { return ArrayIterator<const Block>(beginAt, blkEnd); }
    //! Iterates over all the NVRAM blocks
    //! Invalid blocks are returned as well, make sure to use @ref IsValid before accessing the contents
    constexpr ArrayIterator<const Block> Blocks() { return Blocks(blkStart); }
    //! Iterates over all the blocks with data
    //! Invalid blocks in between are returned, make sure to use @ref IsValid before accessing the contents
    constexpr ArrayIterator<const Block> UsedBlocks() { return Blocks(blkFirst); }

    //! Returns a newly formatted NVRAM block, or NULL if no free space found
    const Block* NewBlock();
    //! Returns a newly formatted NVRAM page, or NULL if no free space found
    const Page* NewPage(ID id, uint32_t recordSize);

private:
    //! The main collector task
    async(Collector);
    //! Executes collectors until at least one page is collected
    int Collect(bool destructive);
    //! Erases all blocks that are marked
    async(EraseBlocks);
};

extern Manager _manager;

//! Simple collector that discards the oldest page, if it exists
const Page* CollectorDiscardOldest(void* arg0, ID key);

}
