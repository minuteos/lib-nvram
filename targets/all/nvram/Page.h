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

    //! Checks if the page contains valid data
    constexpr bool IsValid() const { return id.IsValid(); }
    //! Gets the sequence number of the page
    constexpr uint16_t Sequence() const { return sequence; }

    //! Checks if the payload section of the page is completely empty
    bool IsEmpty() const;

    //! Returns the first page with the specified ID in no particular order
    static const Page* First(ID id);
    //! Returns the next page with the same ID in no particular order
    const Page* Next() const;
    //! Returns the newest page with the specified ID
    static const Page* NewestFirst(ID id) { return (const Page*)RES_PAIR_FIRST(Scan(id)); }
    //! Returns the next older page with the same ID
    const Page* NewestNext() const { return (const Page*)RES_PAIR_FIRST(Scan(id, sequence)); }
    //! Returns the oldest page with the specified ID
    static const Page* OldestFirst(ID id) { return (const Page*)RES_PAIR_SECOND(Scan(id)); }
    //! Returns the next newer page with the same ID
    const Page* OldestNext() const { return (const Page*)RES_PAIR_SECOND(Scan(id, sequence)); }

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

    //! Allocates a new page with the specified ID and optional fixed record size
    static const Page* New(ID id, uint32_t recordSize = 0);

private:
    ID id;
    uint16_t sequence;          //< page sequence number, wraps around
    uint16_t recordSize;        //< fixed record size, or 0 for variable records each prefixed with its size
    uint8_t data[PagePayload];  //< page payload, expected to contain records

    //! Scans the pages with the specified ID, returning a pair of (Newest, Oldest) page
    static res_pair_t Scan(ID id);
    //! Scans the pages with the specified ID, returning a pair of (Older, Newer) page relative to the specified sequence number
    static res_pair_t Scan(ID id, uint16_t seq);
    //! Helper for fast page enumeration, looks for the next page with the specified ID
    static const Page* FastEnum(const nvram::Block* b, const Page* p, ID id);

    static res_pair_t FindUnorderedFirstImpl(ID pageId, uint32_t firstWord);
    static res_pair_t FindUnorderedNextImpl(const uint8_t* rec, uint32_t firstWord);
    static res_pair_t FindForwardNextImpl(const Page* p, const uint8_t* rec, uint32_t firstWord, const Page* (Page::*nextPage)() const);
    static res_pair_t FindNewestFirstImpl(ID page, uint32_t firstWord);
    static res_pair_t FindNewestNextImpl(const uint8_t* stop, uint32_t firstWord);
    static res_pair_t FindNewestNextImpl(const Page* p, const uint8_t* stop, uint32_t firstWord);
    static res_pair_t FindOldestFirstImpl(ID pageId, uint32_t firstWord);
    static res_pair_t FindOldestNextImpl(const uint8_t* rec, uint32_t firstWord);

    static res_pair_t AddFixedImpl(ID page, Span data);
    static res_pair_t AddFixedImpl(ID page, uint32_t firstWord, Span data);
    static res_pair_t AddVarImpl(ID page, Span data);
    static res_pair_t AddVarImpl(ID page, uint32_t firstWord, Span data);
    static res_pair_t ReplaceFixedImpl(ID page, Span data);
    static res_pair_t ReplaceFixedImpl(ID page, uint32_t firstWord, Span data);
    static res_pair_t ReplaceVarImpl(ID page, Span data);
    static res_pair_t ReplaceVarImpl(ID page, uint32_t firstWord, Span data);

    //! Pointer to the start of free space on this page, or NULL if no free space is left
    const uint8_t* FindFree() const;
    //! Compares the relative age of two records
    static int CompareAge(const void* rec1, const void* rec2);

    static res_pair_t AddImpl(ID page, uint32_t firstWord, const void* restOfData, uint32_t totalLengthAndFlags);
    static res_pair_t ReplaceImpl(ID page, uint32_t firstWord, const void* restOfData, uint32_t totalLengthAndFlags);

    static constexpr uint32_t VarGetLen(const void* rec) { return ((const uint32_t*)rec)[-1]; }
    static constexpr uint32_t VarSkipLen(uint32_t payloadLen) { return (payloadLen + 7) & ~3; }
    static constexpr const uint8_t* VarNext(const void* rec) { return (const uint8_t*)rec + VarSkipLen(VarGetLen(rec)); }

    static constexpr uint32_t FirstWord(const void* rec) { return ((const uint32_t*)rec)[0]; }

    static constexpr res_pair_t OffsetPackedData(res_pair_t data, int offset) { return RES_PAIR_FIRST(data) ? RES_PAIR(RES_PAIR_FIRST(data) + offset, RES_PAIR_SECOND(data) - offset) : data; }
};

}
