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

async(Flash::ErasePageAsync, const void* ptr)
async_def()
{
    static struct EraseOperation
    {
        const void* address;
        bool active, done;

        bool PreSleep(mono_t t, mono_t sleepTicks)
        {
            if (sleepTicks < EMULATED_FLASH_ERASE_TICKS)
            {
                return false;
            }

            Flash::Erase(Span(address, PageSize));
            __testrunner_time += EMULATED_FLASH_ERASE_TICKS;
            return done = true;
        }
    } op = { 0 };

    ptr = (const void*)((intptr_t)ptr & ~(PageSize - 1));

    if (!await_acquire_sec(op.active, 1, 1))
    {
        async_return(false);
    }

    op.done = false;
    op.address = ptr;
    kernel::Scheduler::Current().AddPreSleepCallback(op, &EraseOperation::PreSleep);

    if (!await_signal_sec(op.done, 1))
    {
        kernel::Scheduler::Current().RemovePreSleepCallback(op, &EraseOperation::PreSleep);
    }

    op.active = false;
    async_return(op.done);
}
async_end

}
