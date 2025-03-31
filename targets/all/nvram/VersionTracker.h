/*
 * Copyright (c) 2025 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/VersionTracker.h
 */

#pragma once

#include <base/ID.h>

#include <nvram/Manager.h>

namespace nvram
{

class VersionTracker
{
    unsigned version;

public:
    VersionTracker(ID id) { _manager.RegisterVersionTracker(id, &version); }

    async_once(VersionChange) { return async_forward(WaitMaskNot, version, ~0u, version); }
    template<typename T> bool IsCurrentVersion(T& ver)
    {
        if (ver == (T)version) { return true; }
        ver = (T)version;
        return false;
    }
};

}

