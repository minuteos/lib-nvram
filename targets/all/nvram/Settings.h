/*
 * Copyright (c) 2020 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Settings.h
 */

#pragma once

#include <nvram/nvram.h>

#ifndef NVRAM_SETTINGS_NAMES
#if TRACE
#define NVRAM_SETTINGS_NAMES    1
#else
#define NVRAM_SETTINGS_NAMES    0
#endif
#endif

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
    bool IsCurrentVersion(uint16_t& version);

private:
    VariableUniqueKeyStorage storage;
    unsigned version;
    const SettingPtr* first;
    const SettingPtr* last;

    bool InitVersionTracking(uint16_t& version);

    friend class Setting;
};

class SettingSpec
{
public:
    template<typename... TExtra> constexpr SettingSpec(Settings& owner, ID id, const char* name, const void* defVal, size_t defLen, TExtra&&... extra)
        : owner(owner), id(id), defVal(defVal), defLen(defLen)
#if NVRAM_SETTINGS_NAMES
        , name(name)
#endif
#ifdef NVRAM_SETTINGS_EXTRA
        , extra { extra... }
#endif
        {}

    Settings& Owner() const { return owner; }
    ID GetID() const { return id; }
#if NVRAM_SETTINGS_NAMES
    const char* GetName() const { return name; }
#else
    const char* GetName() const { return "???"; }
#endif
#ifdef NVRAM_SETTINGS_EXTRA
    const NVRAM_SETTINGS_EXTRA& Extra() const { return extra; }
#endif
    Span Get() const { return owner.Get(id); }
    Span Set(Span value) const { return owner.Set(id, value); }
    bool Delete() const { return owner.Delete(id); }
    Span DefaultValue() const { return Span(defVal, defLen); }
    size_t ValueLength() const { return defLen; }

public:
    Settings& owner;
    ID id;
    const void* defVal;
    size_t defLen;
#if NVRAM_SETTINGS_NAMES
    const char* name;
#endif
#ifdef NVRAM_SETTINGS_EXTRA
    NVRAM_SETTINGS_EXTRA extra;
#endif
};

template<typename T> class TypedSettingSpec : public SettingSpec
{
public:
    template<typename... TExtra> constexpr TypedSettingSpec(Settings& owner, ID id, const char* name, const void* defVal, size_t defLen, TExtra&&... extra)
        : SettingSpec(owner, id, name, defVal, defLen, extra...) {}
};

class Setting
{
public:
    constexpr Setting(const SettingSpec& spec)
        : spec(spec) {}

    ID GetID() const { return spec.GetID(); }
    const char* GetName() const { return spec.GetName(); }
#ifdef NVRAM_SETTINGS_EXTRA
    const NVRAM_SETTINGS_EXTRA& Extra() const { return spec.Extra(); }
#endif
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

    Span::packed_t GetImpl();
    Span::packed_t SetImpl(Span value);
    Span::packed_t Load();

    friend class Settings;
};

template<typename T> class TypedSetting : public Setting
{
public:
    constexpr TypedSetting(const TypedSettingSpec<T>& spec)
        : Setting(spec) {}

    const T& Get() { return Setting::Get().template Element<T>(); }
    void Set(const T& value) { Setting::Set(value); }
    void SetSpan(Span value) { Setting::Set(value); }

    operator const T&() { return Get(); }
};

template<> class TypedSetting<Span> : public Setting
{
public:
    constexpr TypedSetting(const TypedSettingSpec<Span>& spec)
        : Setting(spec) {}

    operator Span() { return Get(); }
};

template<typename T> struct SettingDefaultType
{
    using t = T;
    template<typename TVal> static constexpr size_t Size(TVal&& value) { return sizeof(TVal); }
};

template<> struct SettingDefaultType<Span>
{
    using t = const char[];
    template<size_t n> static constexpr size_t Size(const char (&literal)[n]) { return n - 1; }
};

}

#define SETTING(group, symbol, type, name, ...) \
    SETTING_DEFAULT(group, symbol, type, name, __VA_ARGS__) \
    SETTING_EX(group, symbol, type, name)

#ifdef NVRAM_SETTING_DEFINITIONS

#define SETTING_GROUP(group, id) \
    __attribute__((section(".rospec.nvs." #group ".tbl"))) const nvram::SettingPtr _nvs_ ## group ## _tbl[] = {}; \
    __attribute__((section(".rospec.nvs." #group ".tbl1"))) const nvram::SettingPtr _nvs_ ## group ## _tbl1[] = {}; \
    INIT_PRIORITY(-9000) nvram::Settings group(id, _nvs_ ## group ## _tbl, _nvs_ ## group ## _tbl1);

#define SETTING_DEFAULT(group, symbol, type, name, ...) \
    __attribute__((section(".rospec.nvs." #group ".def." name))) static const nvram::SettingDefaultType<type>::t _nvs_ ## group ## _def_ ## symbol = __VA_ARGS__;

#define SETTING_EX(group, symbol, type, name, ...) \
    __attribute__((section(".rospec.nvs." #group ".spec." name))) static const nvram::TypedSettingSpec<type> _nvs_ ## group ## _spec_ ## symbol(group, ID::FNV1a(name), name, &_nvs_ ## group ## _def_ ## symbol, nvram::SettingDefaultType<type>::Size(_nvs_ ## group ## _def_ ## symbol), __VA_ARGS__); \
    INIT_PRIORITY(-9000) nvram::TypedSetting<type> symbol(_nvs_ ## group ## _spec_ ## symbol); \
    __attribute__((section(".rospec.nvs." #group ".tbl." name))) const nvram::Setting* _nvs_ ## group ## _ptr_ ## symbol = &symbol;

#else

#define SETTING_GROUP(group, id) \
    extern nvram::Settings group;

#define SETTING_EX(group, symbol, type, name, ...) \
    extern nvram::TypedSetting<type> symbol;

#define SETTING_DEFAULT(group, symbol, type, name, ...)

#endif
