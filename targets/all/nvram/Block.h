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

namespace nvram
{

class Block
{
public:
    //! Returns the address of a NVRAM block from any pointer inside it
    //! The @p ptr is not validated in any way and must point inside a NVRAM block
    ALWAYS_INLINE static const Block* FromPtr(const void* ptr) { return (const Block*)((uintptr_t)ptr & BlockMask); }
    //! Returns a newly formatted NVRAM block, or NULL if no free space found
    static const Block* New();

    //! C++ iterator support - returns pointer to the first @ref Page in the block
    constexpr const class Page* begin() const { return (const class Page*)pages; }
    //! C++ iterator support - returns pointer past the last @ref Page in the block
    constexpr const class Page* end() const { return (const class Page*)padding; }

    //! Determines if a block is valid
    constexpr const bool IsValid() const { return !!generation; }

    struct Enumerator
    {
        constexpr Enumerator(const Block* beginAt = s_first)
            : beginAt(beginAt) {}

        const Block* begin() const { return beginAt; }
        const Block* end() const { return s_end; }

    private:
        const Block* const beginAt;
    };

    //! Iterates over all the blocks or starting at the specified @ref Block.
    //! Invalid blocks in between are returned, make sure to use @ref IsValid before accessing the contents
    static constexpr Enumerator Enumerate(const Block* beginAt = s_first) { return Enumerator(beginAt); }

private:
    //! Magic header (first word) of NVRAM pages
    static constexpr uint32_t Magic = ID("NVRM");  // type has to be uint32_t, Clang is unable to process it as constexpr otherwise

    uint32_t magic;         //< All valid pages must contain the @ref Magic value at offset 0
    uint32_t generation;    //< Number of times this particular Block has been erased
    uint8_t pages[PagesPerBlock][PageSize];
    uint8_t padding[BlockPadding];

    //! Start of the area reserved for NVRAM
    static const Block* s_start;
    //! End of the area reserved for NVRAM (not a valid block)
    static const Block* s_end;
    //! First block containing any data
    static const Block* s_first;

    //! Sets up the area reserved for NVRAM and scans existing Blocks
    static void Initialize(Span area, bool erase);

    //! Determines if the block or its part is empty
    bool IsEmpty(const uint32_t* from = NULL) const;
    //! Writes the block header with the specified generation (erase count) number
    bool Format(uint32_t generation) const;

    friend class Page;
    friend void Initialize(Span, bool);
};

}
