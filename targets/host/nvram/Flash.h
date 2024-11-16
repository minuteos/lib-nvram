/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * host/nvram/Flash.h
 *
 * Emulated flash-like storage
 */

#include <base/base.h>

#include <kernel/kernel.h>

#ifndef EMULATED_FLASH_SIZE
#define EMULATED_FLASH_SIZE     65536
#endif

#ifndef EMULATED_FLASH_PAGE_SIZE
#define EMULATED_FLASH_PAGE_SIZE    4096
#endif

#ifndef EMULATED_FLASH_ERASE_TICKS
#define EMULATED_FLASH_ERASE_TICKS  MonoFromMilliseconds(20)
#endif

namespace nvram
{

class Flash
{
public:
    static constexpr uintptr_t PageSize = EMULATED_FLASH_PAGE_SIZE;

    static Span GetRange();

    static bool Write(const void* ptr, Span data);
#if NVRAM_FLASH_DOUBLE_WRITE
    static bool WriteDouble(const void* ptr, uint32_t lo, uint32_t hi);
    static void ShredDouble(const void* ptr);
#else
    static bool WriteWord(const void* ptr, uint32_t word);
    static void ShredWord(const void* ptr);
#endif
    static bool Erase(Span range);

    static async(ErasePageAsync, const void* ptr);
};

}
