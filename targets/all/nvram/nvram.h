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

namespace nvram
{

//! Initializes the NVRAM, must be called before any other function working with NVRAM
inline void Initialize(Span area = Span(), bool reset = false) { Block::Initialize(area, reset); }

}
