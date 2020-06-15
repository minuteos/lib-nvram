/*
 * Copyright (c) 2020 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Settings.cpp
 */

#include <nvram/Settings.h>

namespace nvram
{

Setting* Settings::GetSetting(ID id)
{
    for (auto setting: *this)
    {
        if (setting->GetID() == id)
            return setting;
    }

    return NULL;
}

Setting* Settings::GetNotifySetting()
{
    for (auto setting: *this)
    {
        if (setting->notify)
            return setting;
    }

    return NULL;
}

bool Settings::IsCurrentVersion(uint16_t& ver)
{
    if (!version)
    {
        return InitVersionTracking(ver);
    }

    if (ver == (uint16_t)version)
    {
        return true;
    }
    else
    {
        ver = version;
        return false;
    }
}

bool Settings::InitVersionTracking(uint16_t& ver)
{
    ver = 1;
    nvram::RegisterVersionTracker(storage.pageId, &version);
    return false;
}

async(Settings::VersionChange)
async_def()
{
    await_mask_not(version, ~0u, version);
}
async_end

res_pair_t Setting::GetImpl()
{
    return spec.Owner().IsCurrentVersion(version) ? (res_pair_t)value : Load();
}

res_pair_t Setting::Load()
{
    auto val = spec.Get();
    if (!val || val.Length() < spec.ValueLength())
    {
        val = spec.DefaultValue();
    }
    if (val != value)
    {
        notify = true;
    }
    return value = val;
}

res_pair_t Setting::SetImpl(Span value)
{
    return value = spec.Set(value);
}

}
