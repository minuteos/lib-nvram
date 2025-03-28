/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Page.h
 */

#pragma once

#include <base/base.h>

#include <nvram/Block.h>

namespace nvram
{

class Page
{
public:
    //! Returns the address of a NVRAM page from any pointer inside it
    //! The @p ptr is not validated in any way and must point inside a NVRAM block
    static const Page* FromPtr(const void* ptr);
    //! Returns the @ref Block that contains the page
    const class Block* Block() const { return (const class Block*)((uintptr_t)this & BlockMask); }

    //! Determines if a page is empty
    constexpr bool IsEmpty() const { return id == ~0u; }
    //! Determines if a page can be erased
    constexpr bool IsErasable() const { return id  == 0; }
    //! Determines if a page is valid
    constexpr bool IsValid() const { return !IsEmpty() && !IsErasable(); }

    //! Gets the sequence number of the page
    constexpr ID GetID() const { return id; }
    //! Gets the sequence number of the page
    constexpr uint16_t Sequence() const { return sequence; }
    //! Gets the free bytes on the page
    uint32_t UnusedBytes() const { auto ptr = FindFree(); return ptr ? data + PagePayload - ptr : 0; }
    //! Gets the used bytes on the page
    uint32_t UsedBytes() const;

    //! Returns the first page with the specified ID in no particular order
    static const Page* First(ID id);
    //! Returns the next page with the same ID in no particular order
    const Page* Next() const { return UnorderedNextImpl(this); }
    //! Returns the newest page with the specified ID
    static const Page* NewestFirst(ID id) { return unpack<FirstScanResult>(Scan(id)).newest; }
    //! Returns the next older page with the same ID
    const Page* NewestNext() const { return unpack<NextScanResult>(Scan(id, this)).older; }
    //! Returns the oldest page with the specified ID
    static const Page* OldestFirst(ID id) { return unpack<FirstScanResult>(Scan(id)).oldest; }
    //! Returns the next newer page with the same ID
    const Page* OldestNext() const { return unpack<NextScanResult>(Scan(id, this)).newer; }
    //! Returns the oldest and newest page with the specified ID
    static void Scan(ID id, const Page*& oldest, const Page*& newest) { auto res = unpack<FirstScanResult>(Scan(id)); newest = res.newest; oldest = res.oldest; }
    //! Returns the next older and newer page with the same ID
    void Scan(const Page*& older, const Page*& newer) const { auto res = unpack<NextScanResult>(Scan(id, this)); newer = res.newer; older = res.older; }

    //! Returns the first record on a page with the specified ID and optional matching first word in no particular order
    static Span FindUnorderedFirst(ID page, uint32_t firstWord = 0) { return FindUnorderedFirstImpl(page, firstWord); }
    //! Returns the next record on a page with the same ID and optional matching first word in no particular order
    static Span FindUnorderedNext(const void* rec, uint32_t firstWord = 0) { return FindUnorderedNextImpl((const uint8_t*)rec, firstWord); }
    //! Returns the newest record on a page with the specified ID and optional matching first word
    static Span FindNewestFirst(ID page, uint32_t firstWord = 0) { return FindNewestFirstImpl(page, firstWord); }
    //! Returns the next older record on a page with the same ID and optional matching first word
    static Span FindNewestNext(const void* rec, uint32_t firstWord = 0) { return FindNewestNextImpl((const uint8_t*)rec, firstWord); }
    //! Returns the oldest record on a page with the specified ID and optional matching first word
    static Span FindOldestFirst(ID page, uint32_t firstWord = 0) { return FindOldestFirstImpl(page, firstWord); }
    //! Returns the next newer record on a page with the same ID and optional matching first word
    static Span FindOldestNext(const void* rec, uint32_t firstWord = 0) { return FindOldestNextImpl((const uint8_t*)rec, firstWord); }

    //! Adds a new record to a page with the specified ID
    //! If a new page is required, a page with fixed size records is allocated
    //! Returns a @ref Span representing the stored record in NVRAM,
    //! or an invalid @ref Span if the record could not be stored
    static Span AddFixed(ID page, Span data) { return AddFixedImpl(page, data); }
    //! Adds a new record to a page with the specified ID
    //! The content of the record is concatenation of the @p firstWord and the contents of the @p data @ref Span
    //! If a new page is required, a page with fixed size records is allocated
    //! Returns a @ref Span representing the stored @p data part in NVRAM,
    //! or an invalid @ref Span if the record could not be stored
    static Span AddFixed(ID page, uint32_t firstWord, Span data) { return AddFixedImpl(page, firstWord, data); }
    //! Adds a new record to a page with the specified ID
    //! If a new page is required, a page with variable size records is allocated
    //! Returns a @ref Span representing the stored record in NVRAM,
    //! or an invalid @ref Span if the record could not be stored
    static Span AddVar(ID page, Span data) { return AddVarImpl(page, data); }
    //! Adds a new record to a page with the specified ID
    //! The content of the record is concatenation of the @p firstWord and the contents of the @p data @ref Span
    //! If a new page is required, a page with variable size records is allocated
    //! Returns a @ref Span representing the stored @p data part in NVRAM,
    //! or an invalid @ref Span if the record could not be stored
    static Span AddVar(ID page, uint32_t firstWord, Span data) { return AddVarImpl(page, firstWord, data); }

    //! Adds a new record to a page with the specified ID, removing all records with the same @p firstWord
    //! If a new page is required, a page with fixed size records is allocated
    //! Returns a @ref Span representing the stored @p data part in NVRAM,
    //! or an invalid @ref Span if the record could not be stored
    static Span ReplaceFixed(ID page, uint32_t firstWord, Span data) { return ReplaceFixedImpl(page, firstWord, data); }
    //! Adds a new record to a page with the specified ID, removing all records with the same @p firstWord
    //! If a new page is required, a page with variable size records is allocated
    //! Returns a @ref Span representing the stored @p data part in NVRAM,
    //! or an invalid @ref Span if the record could not be stored
    static Span ReplaceVar(ID page, uint32_t firstWord, Span data) { return ReplaceVarImpl(page, firstWord, data); }

    //! Deletes all records with the specified @p firstWord from pages with the specified ID
    //! @returns a boolean indicating whethere at least one record was deleted
    static bool Delete(ID page, uint32_t firstWord);

    //! Allocates a new page with the specified ID and optional fixed record size
    static const Page* New(ID id, uint32_t recordSize = 0) { return _manager.NewPage(id, recordSize); }

    //! Tries to move all records from the old page to the new one
    bool MoveRecords(const Page* newPage, size_t limit) const;

    //! Returns the first record on the page
    Span FirstRecord(uint32_t firstWord = 0) const { return FirstRecordImpl(this, firstWord); }
    //! Returns the last record on the page
    Span LastRecord(uint32_t firstWord = 0) const { return LastRecordImpl(this, firstWord); }
    //! Returns the full page data span
    Span PageData() const { return data; }

    //! Enumerates the pages with the specified ID from oldest to newest
    class EnumerateOldestFirst
    {
    public:
        constexpr EnumerateOldestFirst(ID id) : id(id) {}

        class Iterator
        {
        public:
            constexpr bool operator !=(Iterator other) const { return page != other.page; }
            Iterator& operator ++() { page = page->OldestNext(); return *this; }
            constexpr const Page* operator *() const { return page; }

        private:
            const Page* page;

            constexpr Iterator(const Page* page) : page(page) {}

            friend class EnumerateOldestFirst;
        };

        Iterator begin() const { return Iterator(OldestFirst(id)); }
        Iterator end() const { return Iterator(NULL); }

    private:
        ID id;
    };

    //! Enumerates the pages with the specified ID from newest to oldest
    class EnumerateNewestFirst
    {
    public:
        constexpr EnumerateNewestFirst(ID id) : id(id) {}

        class Iterator
        {
        public:
            constexpr bool operator !=(Iterator other) const { return page != other.page; }
            Iterator& operator ++() { page = page->NewestNext(); return *this; }
            constexpr const Page* operator *() const { return page; }

        private:
            const Page* page;

            constexpr Iterator(const Page* page) : page(page) {}

            friend class EnumerateNewestFirst;
        };

        Iterator begin() const { return Iterator(NewestFirst(id)); }
        Iterator end() const { return Iterator(NULL); }

    private:
        ID id;
    };

    class RecordIterator
    {
    public:
        constexpr bool operator !=(RecordIterator other) const { return record.Pointer() != other.record.Pointer(); }
        RecordIterator& operator ++() { record = NextRecordImpl(record); return *this; }
        constexpr Span operator *() const { return record; }

    private:
        Span record;

        constexpr RecordIterator() : record() {}
        constexpr RecordIterator(Span record) : record(record) {}

        friend class Page;
    };

    RecordIterator begin() const { return RecordIterator(FirstRecordImpl(this)); }
    RecordIterator end() const { return RecordIterator(); }

private:
    ID id;
    uint16_t sequence;          //< page sequence number, wraps around
    uint16_t recordSize;        //< fixed record size, or 0 for variable records each prefixed with its size
    uint8_t data[PagePayload];  //< page payload, expected to contain records

    //! Verifies if the page is completely empty
    bool CheckEmpty() const;

    static const Page* UnorderedNextImpl(const Page* after);
    static const Page* OldestNextImpl(const Page* after);
    static const Page* NewestNextImpl(const Page* after);

    struct FirstScanResult
    {
        const Page* newest;
        const Page* oldest;
    };

    struct NextScanResult
    {
        const Page* older;
        const Page* newer;
    };

    //! Scans the pages with the specified ID, returning a pair of (Newest, Oldest) page
    static Packed<FirstScanResult> Scan(ID id);
    //! Scans the pages with the specified ID, returning a pair of (Older, Newer) page relative to the specified page
    static Packed<NextScanResult> Scan(ID id, const Page* p);
    //! Helper for fast page enumeration, looks for the next page with the specified ID
    static const Page* FastEnum(const nvram::Block* b, const Page* p, ID id);

    static Span::packed_t FindUnorderedFirstImpl(ID pageId, uint32_t firstWord);
    static Span::packed_t FindUnorderedNextImpl(const uint8_t* rec, uint32_t firstWord);
    static Span::packed_t FindForwardNextImpl(const Page* p, const uint8_t* rec, uint32_t firstWord, const Page* (*nextPage)(const Page*));
    static Span::packed_t FindNewestFirstImpl(ID page, uint32_t firstWord);
    static Span::packed_t FindNewestNextImpl(const uint8_t* stop, uint32_t firstWord);
    static Span::packed_t FindNewestNextImpl(const Page* p, const uint8_t* stop, uint32_t firstWord, const Page* (*nextPage)(const Page*));
    static Span::packed_t FindOldestFirstImpl(ID pageId, uint32_t firstWord);
    static Span::packed_t FindOldestNextImpl(const uint8_t* rec, uint32_t firstWord);
    static Span::packed_t FirstRecordImpl(const Page* p, uint32_t firstWord = 0);
    static Span::packed_t LastRecordImpl(const Page* p, uint32_t firstWord = 0);
    static Span::packed_t NextRecordImpl(const uint8_t* rec);

    static Span::packed_t AddFixedImpl(ID page, Span data);
    static Span::packed_t AddFixedImpl(ID page, uint32_t firstWord, Span data);
    static Span::packed_t AddVarImpl(ID page, Span data);
    static Span::packed_t AddVarImpl(ID page, uint32_t firstWord, Span data);
    static Span::packed_t ReplaceFixedImpl(ID page, Span data);
    static Span::packed_t ReplaceFixedImpl(ID page, uint32_t firstWord, Span data);
    static Span::packed_t ReplaceVarImpl(ID page, Span data);
    static Span::packed_t ReplaceVarImpl(ID page, uint32_t firstWord, Span data);

    //! Pointer to the start of free space on this page, or NULL if no free space is left
    const uint8_t* FindFree() const;
    //! Compares the relative age of two records
    static int CompareAge(const void* rec1, const void* rec2);

    union LengthAndFlags
    {
        uint32_t raw = 0;
        struct
        {
            uint16_t length;
            uint16_t : 14;
            bool noNotify : 1;
            bool var : 1;
        };

        constexpr LengthAndFlags(uint32_t length)
            : length(uint16_t(length)) {}
        constexpr LengthAndFlags(uint32_t length, bool var)
            : length(uint16_t(length)), var(var) {}
    };

    static Span::packed_t AddImpl(ID page, uint32_t firstWord, const void* restOfData, LengthAndFlags totalLengthAndFlags);
    static Span::packed_t ReplaceImpl(ID page, uint32_t firstWord, const void* restOfData, LengthAndFlags totalLengthAndFlags);
    static Span::packed_t WriteImpl(const uint8_t* free, uint32_t firstWord, const void* restOfData, size_t totalLength);

    static constexpr uint32_t VarGetLen(const void* rec) { return ((const uint32_t*)rec)[-1]; }
    static constexpr uint32_t VarSkipLen(uint32_t payloadLen) { return RequiredAligned(payloadLen + 4); }
    static constexpr const uint8_t* VarNext(const void* rec) { return (const uint8_t*)rec + VarSkipLen(VarGetLen(rec)); }

    static constexpr uint32_t FirstWord(const void* rec) { return ((const uint32_t*)rec)[0]; }

    static constexpr Span::packed_t OffsetPackedData(Span::packed_t data, int offset) { Span res(data); if (res) { res = Span(res.Pointer() + offset, res.Length() - offset); } return res; }

    static RELEASE_ALWAYS_INLINE const Page* FromPtrInline(const void* ptr)
    {
        uintptr_t firstPageInBlock = ((uintptr_t)ptr & BlockMask) + BlockHeader;
        ASSERT(firstPageInBlock > (uintptr_t)_manager.Blocks().begin() && firstPageInBlock < (uintptr_t)_manager.Blocks().end());
        return (const Page*)((uintptr_t)ptr - ((uintptr_t)ptr - firstPageInBlock) % PageSize);
    }

#if NVRAM_FLASH_DOUBLE_WRITE
    static void ShredRecord(const void* ptr);
#else
    static void ShredRecord(const void* ptr) { Flash::ShredWord(ptr); }
#endif

    friend class Manager;
};

}
