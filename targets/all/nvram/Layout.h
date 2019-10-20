/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Layout.h
 *
 * Various constants describing NVRAM layout
 */

#pragma once

#include <base/base.h>

#include <nvram/Flash.h>

namespace nvram
{

//! Erasable block size
constexpr size_t BlockSize = Flash::PageSize;

//! Block pointer alignment mask
constexpr size_t BlockMask = ~(BlockSize - 1);

//! Block header length
constexpr size_t BlockHeader = 8;

//! Pages per block, a bit under 1KB unless overriden
#ifdef NVRAM_PAGES_PER_BLOCK
constexpr size_t PagesPerBlock = NVRAM_PAGES_PER_BLOCK;
#else
constexpr size_t PagesPerBlock = BlockSize / 1024;
#endif

//! Number of pages that are always kept free for allocation
#ifdef NVRAM_PAGES_KEEPT_FREE
constexpr size_t PagesKeptFree = NVRAM_PAGES_KEPT_FREE;
#else
constexpr size_t PagesKeptFree = 4;
#endif

//! Size of individual pages
constexpr size_t PageSize = (BlockSize - BlockHeader) / PagesPerBlock & ~(sizeof(uintptr_t) - 1);

//! Page header length
constexpr size_t PageHeader = 8;

//! Useful page payload
constexpr size_t PagePayload = PageSize - PageHeader;

//! Padding at the end of the block
constexpr size_t BlockPadding = BlockSize - BlockHeader - PagesPerBlock * PageSize;

}
