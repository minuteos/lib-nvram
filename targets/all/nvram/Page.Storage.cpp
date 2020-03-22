/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Page.Storage.cpp
 */

#include <nvram/Page.h>

#define MYDBG(...)  DBGCL("nvram", __VA_ARGS__)

namespace nvram
{

/*!
 * Finds the freshest free space on pages with the specified ID
 */
const uint8_t* Page::FindFree() const
{
    const uint8_t* pe = data + PagePayload;

    if (uint32_t recordSize = this->recordSize)
    {
        for (const uint8_t* rec = data; rec + recordSize <= pe; rec += recordSize)
        {
            if (*(const uint32_t*)rec == ~0u)
            {
                return rec;
            }
        }
    }
    else
    {
        uint32_t len;
        for (const uint8_t* rec = data + 4; rec < pe; rec += VarSkipLen(len))
        {
            len = VarGetLen(rec);
            if (len == ~0u)
            {
                return rec;
            }
        }
    }

    return NULL;
}

/*!
 * Attempts to store a new record in the first free slot. Doesn't consider any other existing records.
 * In case a new page is required, it is allocated with fixed record size large enough to hold
 * the record.
 *
 * The returned data points to the entire record including first word, i.e. the same as passed in data.
 */
res_pair_t Page::AddFixedImpl(ID page, Span data)
{
    ASSERT(data && data.Length());
    const uint32_t* p = data;
    uint32_t firstWord = *p++;
    return AddImpl(page, firstWord, p, data.Length());
}

/*!
 * Attempts to store a new record in the first free slot. Doesn't consider any other existing records.
 *
 * The returned data points to the record excluding the first word, i.e. the same as passed in data.
 */
res_pair_t Page::AddFixedImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(AddImpl(page, firstWord, data, data.Length() + 4), 4);
}

/*!
 * Attempts to store a new record in the first free slot. Doesn't consider any other existing records.
 * In case a new page is required, it is allocated with variable record storage format.
 *
 * The returned data points to the entire record including first word, i.e. the same as passed in data.
 */
res_pair_t Page::AddVarImpl(ID page, Span data)
{
    ASSERT(data || !data.Length());
    const uint32_t* p = data;
    uint32_t firstWord = *p++;
    return AddImpl(page, firstWord, p, data.Length() | BIT(31));
}

/*!
 * Attempts to store a new record in the first free slot. Doesn't consider any other existing records.
 * In case a new page is required, it is allocated with variable record storage format.
 *
 * The returned data points to the record excluding the first word, i.e. the same as passed in data.
 */
res_pair_t Page::AddVarImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(AddImpl(page, firstWord, data, (data.Length() + 4) | BIT(31)), 4);
}

res_pair_t Page::ReplaceFixedImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(ReplaceImpl(page, firstWord, data, data.Length() + 4), 4);
}

res_pair_t Page::ReplaceVarImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(ReplaceImpl(page, firstWord, data, (data.Length() + 4) | BIT(31)), 4);
}

/*!
 * Attempts to store a record to FLASH at the end of the newest page with the specified ID.
 *
 * In case the page format is not suitable or verification fails,
 * more attempts are made and new pages are allocated as needed.
 */
res_pair_t Page::AddImpl(ID page, uint32_t firstWord, const void* restOfData, uint32_t totalLengthAndFlags)
{
    const Page* p = NewestFirst(page);
    const uint8_t* free = p ? p->FindFree() : NULL;

    bool var = GETBIT(totalLengthAndFlags, 31);
    uint32_t totalLength = totalLengthAndFlags & MASK(16);
    uint32_t requiredLength = (totalLength + 3) & ~3;

    ASSERT(totalLength);

    for (;;)
    {
        if (!free ||
            (free + requiredLength > p->data + PagePayload) ||
            (var && p->recordSize) ||
            (!var && p->recordSize && requiredLength > p->recordSize))
        {
            // we need a new page, either because there is not enough free space or a different format is required
            p = New(page, var ? 0 : requiredLength);
            if (!p)
                return Span();
            free = p->data + var * 4;
        }

        if (!p->recordSize)
        {
            // variable records - first reserve space by writing the record length
            if (!Flash::WriteWord(free - 4, totalLength))
            {
                MYDBG("Failed to write length for var record @ %08X", free - 4);
                Flash::ShredWord(free - 4);
                free += 4;	// we can try starting at the next word - since the length is now zero, it will be simply walked over
                continue;
            }
        }

        // the rest is written the same for both record types, with first word written last
        if (totalLength <= 4 || Flash::Write(free + 4, Span(restOfData, totalLength - 4)))
        {
            if (Flash::WriteWord(free, firstWord))
            {
                _manager.Notify(page);
                return Span(free, totalLength);
            }
        }

        MYDBG("Failed to write record @ %08X", free);
        Flash::ShredWord(free);

        free += p->recordSize ? p->recordSize : VarSkipLen(totalLength);
    }
}

/*!
 * Ensures that the provided record is the only one stored with the specified key (i.e. firstWord).
 *
 * If the newest stored instance of the record is the same as the new one provided, it is *not* written again.
 */
res_pair_t Page::ReplaceImpl(ID page, uint32_t firstWord, const void* restOfData, uint32_t totalLengthAndFlags)
{
    Span rec = FindUnorderedFirst(page, firstWord);

    if (!rec)
    {
        // no previous record exists, simply add a new one
        return AddImpl(page, firstWord, restOfData, totalLengthAndFlags);
    }

    // the one found might not be the only one
    while (Span next = FindUnorderedNext(rec, firstWord))
    {
        MYDBG("Multiple records with the same key found @ %08X and %08X", (const void*)rec, (const void*)next);
        const void* del;
        if (CompareAge(rec, next) < 0)
        {
            del = rec;
            rec = next;
        }
        else
        {
            del = next;
        }

        MYDBG("Deleting older: %08X", del);
        Flash::ShredWord(del);
    }

    uint32_t len = totalLengthAndFlags & MASK(16);

    if (rec.Length() >= len && (len <= 4 || !memcmp(restOfData, rec.Pointer() + 4, len - 4)))
    {
        // the record is the same - it might be longer, but we care only about the part that was about to be written
        MYDBG("Same record already written @ %08X", rec);
        return rec;
    }

    res_pair_t res = AddImpl(page, firstWord, restOfData, totalLengthAndFlags);

    if (RES_PAIR_FIRST(res))
    {
        // delete the previous record if the new one has been written successfully
        Flash::ShredWord(rec);
    }

    return res;
}

}
