/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/nvram.h
 *
 * Main header for non-volatile storage
 */

#pragma once

#include <kernel/kernel.h>

#include <nvram/Page.h>
#include <nvram/Block.h>
#include <nvram/Storage.h>
#include <nvram/Manager.h>

namespace nvram
{

//! Initializes the NVRAM, must be called before any other function working with NVRAM
inline void Initialize(Span area = Span(), bool reset = false) { _manager.Initialize(area, reset); }

//! Enumerates all NVRAM blocks
inline ArrayIterator<const Block> Blocks() { return _manager.Blocks(); }

//! Enumerates potentially used NVRAM blocks
inline ArrayIterator<const Block> UsedBlocks() { return _manager.UsedBlocks(); }

//! Registers a NVRAM collector
inline void RegisterCollector(ID pageId, unsigned level, CollectorDelegate collector) { _manager.RegisterCollector(pageId, level, collector); }

}
