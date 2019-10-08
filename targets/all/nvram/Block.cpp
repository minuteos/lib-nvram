/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Block.cpp
 *
 * NVRAM block (erasable unit) management
 */

#include <nvram/Block.h>

#define MYDBG(...)  DBGCL("nvram", __VA_ARGS__)

namespace nvram
{

const Block* Block::s_start;
const Block* Block::s_end;
const Block* Block::s_first;

/*!
 * Scans the area reserved for NVRAM for existing Blocks,
 * fixes any problems that may have been created by an unexpected reset,
 * so that we can make some assumptions about the flash layout
 * and not bother with them when doing scans in the future
 */
void Block::Initialize(Span area, bool erase)
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
    s_start = (const Block*)(((uintptr_t)area.begin() + ~BlockMask) & BlockMask);
    s_first = s_end = (const Block*)((uintptr_t)area.end() & BlockMask);

    ASSERT(s_start < s_end);

    if (erase)
    {
        MYDBG("Erasing NVRAM area %p - %p", area.begin(), area.end());
        Flash::Erase(area);
    }

    for (auto* blk = s_end - 1; blk >= s_start; blk--)
    {
        if (blk->magic == Magic)
        {
            s_first = blk;

            if (blk->generation == ~0u)
            {
                // appears to be a half-initialized sector
                MYDBG("Found half-initialized sector (magic but no generation) @ %08X", blk);
                if (blk->IsEmpty(&blk->generation + 1))
                {
                    // try to initialize
                    if (blk->Format(1))
                    {
                        // success
                        continue;
                    }
                }

                // mark it as corrupted, there is something wrong with it
                MYDBG("ERROR - Failed to complete block initialization @ %08X\n", blk);
                Flash::ShredWord(&blk->generation);
            }
        }
        else if (!blk->IsEmpty())
        {
            // any non-empty sector without a magic header is unexpected a we won't touch it or anything before it
            MYDBG("WARNING - Unexpected data @ %08X, limiting range", blk);
            s_start = blk + 1;
            break;
        }
    }

    MYDBG("Init complete - %08X <= %08X <= %08X\n", s_start, s_first, s_end);
}

bool Block::IsEmpty(const uint32_t* from) const
{
    const uint32_t* p = from ? from : &magic;
    const uint32_t* e = &this[1].magic;
    while (p != e)
    {
        if (*p++ != ~0u)
            return false;
    }

    return true;
}

bool Block::Format(uint32_t gen) const
{
    if (Flash::WriteWord(&magic, Magic) &&
        Flash::WriteWord(&generation, gen))
    {
        MYDBG("Formatted sector gen %d @ %08X", gen, this);
        return true;
    }

    Flash::ShredWord(&generation);
    Flash::ShredWord(&magic);
    MYDBG("ERROR - Failed to format sector gen %d @ %08X", gen, this);
    return false;
}

const Block* Block::New()
{
    // we can use only empty blocks, erase is too time consuming to perform synchronously
    for (auto* blk = s_end - 1; blk >= s_start; blk--)
    {
        if (blk->IsEmpty() && blk->Format(1))
        {
            if (s_first > blk)
                s_first = blk;
            return blk;
        }
    }

    return NULL;
}

}
