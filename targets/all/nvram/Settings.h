/*
 * Copyright (c) 2020 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Settings.h
 */

#pragma once

#include <nvram/nvram.h>

namespace nvram
{

class Setting;

typedef Setting* SettingPtr;
typedef const SettingPtr* SettingIterator;

class Settings
{
public:
    constexpr Settings(ID id, const SettingPtr* first, const SettingPtr* last)
        : storage(id), version(~0), first(first), last(last)
    {
    }

    Setting* GetSetting(ID id);
    Setting* GetNotifySetting();
    Span Get(ID id) const { return storage.Get(id); }
    Span Set(ID id, Span value) const { return storage.Set(id, value); }
    bool Delete(ID id) const { return storage.Delete(id); }

    const SettingPtr* begin() const { return first; }
    const SettingPtr* end() const { return last; }

    async(VersionChange);

private:
    VariableUniqueKeyStorage storage;
    unsigned version;
    const SettingPtr* first;
    const SettingPtr* last;

    bool IsCurrentVersion(uint16_t& version);
    bool InitVersionTracking(uint16_t& version);

    friend class Setting;
};

class SettingSpec
{
public:
    constexpr SettingSpec(Settings& owner, ID id, const char* name, Span defaultValue)
        : owner(owner), id(id),
#if TRACE
        name(name),
#endif
        defaultValue(defaultValue) {}

    Settings& Owner() const { return owner; }
    ID GetID() const { return id; }
#if TRACE
    const char* GetName() const { return name; }
#else
    const char* GetName() const { return "???"; }
#endif
    Span Get() const { return owner.Get(id); }
    Span Set(Span value) const { return owner.Set(id, value); }
    bool Delete() const { return owner.Delete(id); }
    Span DefaultValue() const { return defaultValue; }
    size_t ValueLength() const { return defaultValue.Length(); }

private:
    Settings& owner;
    ID id;
#if TRACE
    const char* name;
#endif
    Span defaultValue;
};

template<typename T> class TypedSettingSpec : public SettingSpec
{
public:
    constexpr TypedSettingSpec(Settings& owner, ID id, const char* name, const T& defaultValue = {})
        : SettingSpec(owner, id, name, defaultValue) {}
};

class Setting
{
public:
    constexpr Setting(const SettingSpec& spec)
        : spec(spec) {}

    ID GetID() const { return spec.GetID(); }
    const char* GetName() const { return spec.GetName(); }
    Span Get() { return GetImpl(); }
    Span GetSpan() { return GetImpl(); }
    Span Set(Span value) { return SetImpl(value); }
    bool Delete() { return spec.Delete(); }

    size_t ValueLength() const { return spec.ValueLength(); }
    void MarkNotified() { notify = false; }

private:
    const SettingSpec& spec;
    uint16_t version = 0;
    bool notify = false;
    Span value;

    RES_PAIR_DECL(GetImpl);
    RES_PAIR_DECL(SetImpl, Span value);
    RES_PAIR_DECL(Load);

    friend class Settings;
};

template<typename T> class TypedSetting : public Setting
{
public:
    constexpr TypedSetting(const TypedSettingSpec<T>& spec)
        : Setting(spec) {}

    const T& Get() { return Setting::Get().Element<T>(); }
    void Set(const T& value) { Setting::Set(value); }

    operator const T&() { return Get(); }
};

template<> class TypedSetting<Span> : public Setting
{
public:
    constexpr TypedSetting(const TypedSettingSpec<Span>& spec)
        : Setting(spec) {}

    operator Span() { return Get(); }
};

}

#ifdef NVRAM_SETTING_DEFINITIONS

#define SETTING_GROUP(group, id) \
    __attribute__((section(".rospec.nvs." #group ".tbl"))) const nvram::SettingPtr _nvs_ ## group ## _tbl[] = {}; \
    __attribute__((section(".rospec.nvs." #group ".tbl1"))) const nvram::SettingPtr _nvs_ ## group ## _tbl1[] = {}; \
    nvram::Settings group(id, _nvs_ ## group ## _tbl, _nvs_ ## group ## _tbl1);

#define SETTING(group, symbol, type, name, ...) \
    __attribute__((section(".rospec.nvs." #group ".def." name))) static const type _nvs_ ## group ## _def_ ## symbol = type(__VA_ARGS__); \
    __attribute__((section(".rospec.nvs." #group ".spec." name))) static const nvram::TypedSettingSpec<type> _nvs_ ## group ## _spec_ ## symbol(group, ID::FNV1a(name), name, _nvs_ ## group ## _def_ ## symbol); \
    nvram::TypedSetting<type> symbol(_nvs_ ## group ## _spec_ ## symbol); \
    __attribute__((section(".rospec.nvs." #group ".tbl." name))) const nvram::Setting* _nvs_ ## group ## _ptr_ ## symbol = &symbol;

#else

#define SETTING_GROUP(group, id) \
    extern nvram::Settings group;

#define SETTING(group, symbol, type, name, ...) \
    extern nvram::TypedSetting<type> symbol;

#endif
