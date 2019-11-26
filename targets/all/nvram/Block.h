/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Block.h
 */

#pragma once

#include <base/base.h>
#include <base/ID.h>

#include <nvram/Layout.h>
#include <nvram/Manager.h>

namespace nvram
{

class Block
{
public:
    //! Returns the address of a NVRAM block from any pointer inside it
    //! The @p ptr is not validated in any way and must point inside a NVRAM block
    ALWAYS_INLINE static const Block* FromPtr(const void* ptr) { return (const Block*)((uintptr_t)ptr & BlockMask); }
    //! Returns a newly formatted NVRAM block, or NULL if no free space found
    static const Block* New() { return _manager.NewBlock(); }

    //! C++ iterator support - returns pointer to the first @ref Page in the block
    constexpr const class Page* begin() const { return (const class Page*)pages; }
    //! C++ iterator support - returns pointer past the last @ref Page in the block
    constexpr const class Page* end() const { return (const class Page*)padding; }

    //! Gets block generation (erase count)
    constexpr const uint32_t Generation() const { return generation; }
    //! Determines if a block is empty
    constexpr const bool IsEmpty() const { return magic == ~0u; }
    //! Determines if a block is scheduled to be erased
    constexpr const bool IsErasable() const { return magic == 0; }
    //! Determines if a block is valid
    constexpr const bool IsValid() const { return !IsEmpty() && !IsErasable(); }

private:
    //! Magic header (first word) of NVRAM pages
    static constexpr uint32_t Magic = ID("NVRM");  // type has to be uint32_t, Clang is unable to process it as constexpr otherwise

    //! Types of pages found in block
    enum PageFlags
    {
        PagesErasable = 1,
        PagesUsed = 2,
        PagesFree = 4,
    };

    uint32_t magic;         //< All valid pages must contain the @ref Magic value at offset 0
    uint32_t generation;    //< Number of times this particular Block has been erased
    uint8_t pages[PagesPerBlock][PageSize];
    uint8_t padding[BlockPadding];

    //! Sets up the area reserved for NVRAM and scans existing Blocks
    static void Initialize(Span area, bool erase);

    //! Checks the contents of the block to determine if it is empty
    bool CheckEmpty(const uint32_t* from = NULL) const;
    //! Checks the contents of the block, returning a res_pair_t of (PageFlags, freeCount)
    RES_PAIR_DECL_EX(CheckPages, const);
    //! Writes the block header with the specified generation (erase count) number
    bool Format(uint32_t generation) const;

    friend class Page;
    friend class Manager;
};

}
