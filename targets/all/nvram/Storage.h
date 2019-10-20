/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Storage.h
 *
 * Helpers for simple sequential storage
 */

#pragma once

#include <nvram/Page.h>

namespace nvram
{

//! Helper for NVRAM storage pages with fixed size records
template<typename T> struct FixedStorage
{
    constexpr FixedStorage(ID pageId = T::PageID) : pageId(pageId) {}

    //! Returns the first record in no specific order
    const T* UnorderedFirst() const { return (const T*)Page::FindUnorderedFirst(pageId); }
    //! Returns the next record in no specific order
    const T* UnorderedNext(const T* after) const { return (const T*)Page::FindUnorderedNext(after); }
    //! Returns the newest record
    const T* NewestFirst() const { return (const T*)Page::FindNewestFirst(pageId); }
    //! Returns the next older record
    const T* NewestNext(const T* after) const { return (const T*)Page::FindNewestNext(after); }
    //! Returns the oldest record
    const T* OldestFirst() const { return (const T*)Page::FindOldestFirst(pageId); }
    //! Returns the next newer record
    const T* OldestNext(const T* after) const { return (const T*)Page::FindOldestNext(after); }

    //! Adds a new record, returns pointer to the new record in NVRAM or NULL if the record could not be written
    const T* Add(const T* record) const { return Page::AddFixed(pageId, Span(record, sizeof(T))); }
    //! Adds a new record, returns pointer to the new record in NVRAM or NULL if the record could not be written
    const T* Add(const T& record) const { return Add(&record); }

    const uint32_t pageId;
};

//! Helper for NVRAM storage pages with variable size records
struct VariableStorage
{
    constexpr VariableStorage(ID pageId) : pageId(pageId) {}

    //! Returns the first record in no specific order
    Span UnorderedFirst() const { return Page::FindUnorderedFirst(pageId); }
    //! Returns the next record in no specific order
    Span UnorderedNext(const void* after) const { return Page::FindUnorderedNext(after); }
    //! Returns the newest record
    Span NewestFirst() const { return Page::FindNewestFirst(pageId); }
    //! Returns the next older record
    Span NewestNext(const void* after) const { return Page::FindNewestNext(after); }
    //! Returns the oldest record
    Span OldestFirst() const { return Page::FindOldestFirst(pageId); }
    //! Returns the next newer record
    Span OldestNext(const void* after) const { return Page::FindOldestNext(after); }

    //! Adds a new record, returns a @ref Span representing the new record in NVRAM,
    //! or an invalid @ref Span if the record could not be written
    Span Add(Span data) const { return Page::AddVar(pageId, data); }

    const uint32_t pageId;
};

//! Helper for NVRAM storage pages with fixed size records identified by 32-bit keys
template<typename T> struct FixedKeyStorage
{
    constexpr FixedKeyStorage(ID pageId = T::PageID) : pageId(pageId) {}

    //! Returns the first record with the specified key, in no specific order
    const T* UnorderedFirst(ID key) const { return KeyToPtr(Page::FindUnorderedFirst(pageId, key)); }
    //! Returns the next record with the same key in no specific order
    const T* UnorderedNext(const T* after) const { return KeyToPtr(Page::FindUnorderedNext(PtrToKey(after), KeyFromPtr(after))); }
    //! Returns the newest record with the specified key
    const T* NewestFirst(ID key) const { return KeyToPtr(Page::FindNewestFirst(pageId, key)); }
    //! Returns the next older record with the same key
    const T* NewestNext(const T* after) const { return KeyToPtr(Page::FindNewestNext(PtrToKey(after), KeyFromPtr(after))); }
    //! Returns the oldest record with the specified key
    const T* OldestFirst(ID key) const { return KeyToPtr(Page::FindOldestFirst(pageId, key)); }
    //! Returns the next newer record with the same key
    const T* OldestNext(const T* after) const { return KeyToPtr(Page::FindOldestNext(PtrToKey(after), KeyFromPtr(after))); }

    //! Returns the first record and its associated key in no specified order
    const T* EnumerateUnorderedFirst(ID& key) const { return KeyToPtr(Page::FindUnorderedFirst(pageId), key); }
    //! Returns the next record and its associated key in no specified order
    const T* EnumerateUnorderedNext(const T* after, ID& key) const { return KeyToPtr(Page::FindUnorderedNext(PtrToKey(after)), key); }

    //! Adds a new record with the specified key, returns a pointer to the new record in NVRAM,
    //! or NULL if the record could not be written
    const T* Add(ID key, const T* record) const { return Page::AddFixed(pageId, key, Span(record, sizeof(T))); }
    //! Adds a new record with the specified key, returns a pointer to the new record in NVRAM,
    //! or NULL if the record could not be written
    const T* Add(ID key, const T& record) const { return Add(key, &record); }
    //! Replaces all records with the specified key with a new one,
    //! returns a pointer to the new record in NVRAM,
    //! or NULL if the record could not be written
    const T* Replace(ID key, const T* record) const { return Page::ReplaceFixed(pageId, key, Span(record, sizeof(T))); }
    //! Replaces all records with the specified key with a new one,
    //! returns a pointer to the new record in NVRAM,
    //! or NULL if the record could not be written
    const T* Replace(ID key, const T& record) const { return Replace(key, &record); }

    const uint32_t pageId;

private:
    static constexpr const T* KeyToPtr(const void* rec) { return rec ? (const T*)((const uint8_t*)rec + 4) : NULL; }
    static constexpr const T* KeyToPtr(const void* rec, ID& key) { return rec ? ({ key = *(const uint32_t*)rec; (const T*)((const uint8_t*)rec + 4); }) : NULL; }
    static constexpr const void* PtrToKey(const T* ptr) { return ptr ? (const uint8_t*)ptr - 4 : NULL; }
    static constexpr const uint32_t KeyFromPtr(const T* ptr) { return ((const uint32_t*)ptr)[-1]; }
};

//! Helper for NVRAM storage pages with variable size records identified by 32-bit keys
struct VariableKeyStorage
{
    constexpr VariableKeyStorage(ID pageId) : pageId(pageId) {}

    //! Returns the first record with the specified key, in no specific order
    Span UnorderedFirst(ID key) const { return DataWithoutKey(Page::FindUnorderedFirst(pageId, key)); }
    //! Returns the next record with the same key in no specific order
    Span UnorderedNext(const void* after) const { return DataWithoutKey(Page::FindUnorderedNext(PtrToKey(after), KeyFromPtr(after))); }
    //! Returns the newest record with the specified key
    Span NewestFirst(ID key) const { return DataWithoutKey(Page::FindNewestFirst(pageId, key)); }
    //! Returns the oldest record with the specified key
    Span NewestNext(const void* after) const { return DataWithoutKey(Page::FindNewestNext(PtrToKey(after), KeyFromPtr(after))); }
    //! Returns the oldest record with the specified key
    Span OldestFirst(ID key) const { return DataWithoutKey(Page::FindOldestFirst(pageId, key)); }
    //! Returns the next newer record with the same key
    Span OldestNext(const void* after) const { return DataWithoutKey(Page::FindOldestNext(PtrToKey(after), KeyFromPtr(after))); }

    //! Returns the first record and its associated key in no specified order
    Span EnumerateUnorderedFirst(ID& key) const { return DataWithoutKey(Page::FindUnorderedFirst(pageId), key); }
    //! Returns the next record and its associated key in no specified order
    Span EnumerateUnorderedNext(const void* after, ID& key) const { return DataWithoutKey(Page::FindUnorderedNext(PtrToKey(after)), key); }

    //! Adds a new record with the specified key, returns a @ref Span representing the new record in NVRAM,
    //! or an invalid @ref Span if the record could not be written
    Span Add(ID key, Span data) const { return Page::AddVar(pageId, key, data); }
    //! Replaces all records with the specified key with a new one,
    //! returns a @ref Span representing the new record in NVRAM,
    //! or an invalid @ref Span if the record could not be written
    Span Replace(ID key, Span data) const { return Page::ReplaceVar(pageId, key, data); }

    const uint32_t pageId;

private:
    static constexpr Span DataWithoutKey(Span data) { return data ? Span(data.Pointer() + 4, data.Length() - 4) : data; }
    static constexpr Span DataWithoutKey(Span data, ID& key) { return data ? ({ key = *(const uint32_t*)data; Span(data.Pointer() + 4, data.Length() - 4); }) : data; }
    static constexpr const void* PtrToKey(const void* ptr) { return ptr ? (const uint8_t*)ptr - 4 : NULL; }
    static constexpr const uint32_t KeyFromPtr(const void* ptr) { return ((const uint32_t*)ptr)[-1]; }
};

//! Helper for NVRAM storage pages with fixed size records identified by unique 32-bit keys
template<typename T> struct FixedUniqueKeyStorage
{
    constexpr FixedUniqueKeyStorage(ID pageId = T::PageID) : pageId(pageId) {}

    //! Returns the record with the specified key, or NULL if record not found
    const T* Get(ID key) const { return KeyToPtr(Page::FindUnorderedFirst(pageId, key)); }
    //! Stores the record with the specified key,
    //! returns a pointer to the new record in NVRAM,
    //! or NULL if the record could not be written
    const T* Set(ID key, const T* record) const { return Page::ReplaceFixed(pageId, key, Span(record, sizeof(T))); }
    //! Stores the record with the specified key,
    //! returns a pointer to the new record in NVRAM,
    //! or NULL if the record could not be written
    const T* Set(ID key, const T& record) const { return Set(key, &record); }

    const uint32_t pageId;

private:
    static constexpr const T* KeyToPtr(const void* rec) { return rec ? (const T*)((const uint8_t*)rec + 4) : NULL; }
};

struct VariableUniqueKeyStorage
{
    constexpr VariableUniqueKeyStorage(ID pageId) : pageId(pageId) {}

    //! Returns the record with the specified key, or an invalid @ref Span if record not found
    Span Get(ID key) const { return DataWithoutKey(Page::FindUnorderedFirst(pageId, key)); }
    //! Stores the record with the specified key,
    //! returns a @ref Span representing the new record in NVRAM,
    //! or an invalid @ref Span if the record could not be written
    Span Set(ID key, Span data) const { return Page::ReplaceVar(pageId, key, data); }

    const uint32_t pageId;

private:
    static constexpr Span DataWithoutKey(Span data) { return data ? Span(data.Pointer() + 4, data.Length() - 4) : data; }
};

}
