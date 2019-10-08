/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * host/nvram/Flash.cpp
 *
 * Emulated flash-like storage
 */

#include "Flash.h"

#include <sys/mman.h>

namespace nvram
{

static struct FlashImpl
{
    FlashImpl()
    {
        p = mmap(NULL, Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        memset(p, 0xFF, Size);
        Protect();
    }

    void Protect() { mprotect(p, Size, PROT_READ); }
    void Unprotect() { mprotect(p, Size, PROT_READ | PROT_WRITE); }

    static constexpr size_t Size = EMULATED_FLASH_SIZE;
    static constexpr size_t PageSize = EMULATED_FLASH_PAGE_SIZE;

    void* p;
} flash;

Span Flash::GetRange()
{
    return Span(flash.p, flash.Size);
}

bool Flash::Write(const void* ptr, Span data)
{
    flash.Unprotect();
    char* p = (char*)ptr;
    for (char ch: data)
        *p++ &= ch;
    flash.Protect();
    return true;
}

void Flash::ShredWord(const void* ptr)
{
    flash.Unprotect();
    *(uint32_t*)ptr = 0;
    flash.Protect();
}

bool Flash::WriteWord(const void* ptr, uint32_t word)
{
    flash.Unprotect();
    *(uint32_t*)ptr &= word;
    flash.Protect();
    return *(uint32_t*)ptr == word;
}

bool Flash::Erase(Span range)
{
    flash.Unprotect();
    memset((void*)range.Pointer(), 0xFF, range.Length());
    flash.Protect();
    return true;
}

}
