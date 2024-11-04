/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Block.cpp
 *
 * NVRAM block (erasable unit) management
 */

#include <nvram/nvram.h>

#define MYDBG(...)  DBGCL("nvram", __VA_ARGS__)

namespace nvram
{

bool Block::CheckEmpty(const uint32_t* from) const
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

Packed<Block::CheckResult> Block::CheckPagesImpl() const
{
    uint32_t flags = 0;
    uint32_t freeCount = 0;

    for (auto p: *this)
    {
        if (p.IsErasable())
        {
            flags |= PagesErasable;
        }
        else if (p.IsEmpty())
        {
            freeCount++;
            flags |= PagesFree;
        }
        else
        {
            flags |= PagesUsed;
        }
    }

    return pack<CheckResult>(flags, freeCount);
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

}
