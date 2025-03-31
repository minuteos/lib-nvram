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
    PLATFORM_CRITICAL_SECTION();
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
Span::packed_t Page::AddFixedImpl(ID page, Span data)
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
Span::packed_t Page::AddFixedImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(AddImpl(page, firstWord, data, data.Length() + 4), 4);
}

/*!
 * Attempts to store a new record in the first free slot. Doesn't consider any other existing records.
 * In case a new page is required, it is allocated with variable record storage format.
 *
 * The returned data points to the entire record including first word, i.e. the same as passed in data.
 */
Span::packed_t Page::AddVarImpl(ID page, Span data)
{
    ASSERT(data || !data.Length());
    const uint32_t* p = data;
    uint32_t firstWord = *p++;
    return AddImpl(page, firstWord, p, LengthAndFlags(data.Length(), true));
}

/*!
 * Attempts to store a new record in the first free slot. Doesn't consider any other existing records.
 * In case a new page is required, it is allocated with variable record storage format.
 *
 * The returned data points to the record excluding the first word, i.e. the same as passed in data.
 */
Span::packed_t Page::AddVarImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(AddImpl(page, firstWord, data, LengthAndFlags(data.Length() + 4, true)), 4);
}

Span::packed_t Page::ReplaceFixedImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(ReplaceImpl(page, firstWord, data, data.Length() + 4), 4);
}

Span::packed_t Page::ReplaceVarImpl(ID page, uint32_t firstWord, Span data)
{
    return OffsetPackedData(ReplaceImpl(page, firstWord, data, LengthAndFlags(data.Length() + 4, true)), 4);
}

/*!
 * Attempts to store a record to FLASH at the end of the newest page with the specified ID.
 *
 * In case the page format is not suitable or verification fails,
 * more attempts are made and new pages are allocated as needed.
 */
Span::packed_t Page::AddImpl(ID page, uint32_t firstWord, const void* restOfData, LengthAndFlags totalLengthAndFlags)
{
    PLATFORM_CRITICAL_SECTION();
    const Page* p = NewestFirst(page);
    const uint8_t* free = p ? p->FindFree() : NULL;

    bool var = totalLengthAndFlags.var;
    uint32_t totalLength = totalLengthAndFlags.length;
    uint32_t requiredLength = RequiredAligned(totalLength);

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

        auto res = Span(WriteImpl(free, firstWord, restOfData, totalLength));
        if (res)
        {
            if (!totalLengthAndFlags.noNotify)
            {
                _manager.Notify(page);
            }
            return res;
        }

        free = NULL;
    }
}

/*!
 * Ensures that the provided record is the only one stored with the specified key (i.e. firstWord).
 *
 * If the newest stored instance of the record is the same as the new one provided, it is *not* written again.
 */
Span::packed_t Page::ReplaceImpl(ID page, uint32_t firstWord, const void* restOfData, LengthAndFlags totalLengthAndFlags)
{
    PLATFORM_CRITICAL_SECTION();
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
        ShredRecord(del);
    }

    uint32_t len = totalLengthAndFlags.length;
    bool var = totalLengthAndFlags.var;

    if ((rec.Length() == len || (!var && rec.Length() > len)) &&
        (len <= 4 || !memcmp(restOfData, rec.Pointer() + 4, len - 4)))
    {
        // the record is the same - if using fixed size records, it might be longer, but we care only about the part that was about to be written
        MYDBG("Same record already written @ %08X", rec);
        return rec;
    }

    totalLengthAndFlags.noNotify = true;    // suppress notification when adding, notify after deleting the previous record
    Span res = AddImpl(page, firstWord, restOfData, totalLengthAndFlags);

    if (res)
    {
        // delete the previous record if the new one has been written successfully
        ShredRecord(rec);
    }

    _manager.Notify(page);

    return res;
}

/*!
 * Tries to write the record, starting at the specified location
 *
 * Returns the Span of the new record on success or an empty Span if the record
 * couldn't be written anywhere until the end of the page
 */
Span::packed_t Page::WriteImpl(const uint8_t* free, uint32_t firstWord, const void* restOfData, size_t totalLength)
{
    PLATFORM_CRITICAL_SECTION();
    const Page* p = FromPtrInline(free);

    for (;;)
    {
#if NVRAM_FLASH_DOUBLE_WRITE
        if (p->recordSize)
        {
            // record size is already validated, just make sure there is still enough free space
            if (free + p->recordSize > endof(p->data))
            {
                return {};
            }

            // make sure the target span is free from unfinished writes
            if (Span(free, RequiredAligned(totalLength)).IsAllOnes())
            {
                // start by writing everything but the first doubleword
                if (totalLength <= 8 || Flash::Write(free + 8, Span((const uint8_t*)restOfData + 4, totalLength - 8)))
                {
                    // write first doubleword last
                    if (Flash::WriteDouble(free, firstWord, *(const uint32_t*)restOfData))
                    {
                        // success
                        return Span(free, totalLength);
                    }
                }
            }

            // failed, just shred the first dword to skip over the corrupted record
            MYDBG("Failed to write fixed record @ %08X", free);
            Flash::ShredDouble(free);
            free += p->recordSize;
        }
        else
        {
            auto end = free - 4 + RequiredAligned(totalLength + 4);

            if (end > endof(p->data))
            {
                // record won't fit
                return {};
            }

            // With doublewords, we cannot start by reserving the length first to avoid having
            // a valid looking but unfinished record in case of power loss, so we just write the payload first
            // this also means we need to make sure there are no accidental unfinished writes in the target
            // span before writing
            // also verify the next word, if it doesn't reach the end of the page, to make sure we don't
            // create a record that makes corrupted data accessible
            if (end < endof(p->data))
            {
                end += 8;
            }

            while (end > free && ((const uint64_t*)end)[-1] + 1 == 0)
            {
                end -= 8;
            }

            if (end > free)
            {
                MYDBG("Failed to write variable record @ %08X, found garbage @ %08X: %8H", free, end - 8, Span(end - 8, 8));

                // shred the garbage and continue after that
                auto newFree = end + 4;
                while (end > free)
                {
                    Flash::ShredDouble(end - 8);
                    end -= 8;
                }

                free = newFree;
                continue;
            }

            if (totalLength <= 4 || Flash::Write(free + 4, Span(restOfData, totalLength - 4)))
            {
                // write first doubleword
                if (Flash::WriteDouble(free - 4, totalLength, firstWord))
                {
                    // success
                    return Span(free, totalLength);
                }
            }

            // simply retry - any garbage will be detected and repaired
            MYDBG("Failed to write variable record @ %08X", free);
        }
#else
        if (p->recordSize)
        {
            // record size is already validated, just make sure there is still enough free space
            if (free + p->recordSize > endof(p->data))
            {
                return {};
            }
        }
        else
        {
            uint32_t requiredLength = RequiredAligned(totalLength);

            for (;;)
            {
                if (free + requiredLength > endof(p->data))
                {
                    return {};
                }

                // variable records - first reserve space by writing the record length
                if (Flash::WriteWord(free - 4, totalLength))
                {
                    break;
                }

                MYDBG("Failed to write length for var record @ %08X", free - 4);
                Flash::ShredWord(free - 4);
                free += 4;	// we can try starting at the next word - since the length is now zero, it will be simply walked over
            }
        }

        // the rest is written the same for both record types, with first word written last
        if (totalLength <= 4 || Flash::Write(free + 4, Span(restOfData, totalLength - 4)))
        {
            if (Flash::WriteWord(free, firstWord))
            {
                // success - return the span of the written record
                return Span(free, totalLength);
            }
        }

        MYDBG("Failed to write record @ %08X", free);
        ShredRecord(free);
        free += p->recordSize ? p->recordSize : VarSkipLen(totalLength);
#endif
    }
}

#if NVRAM_FLASH_DOUBLE_WRITE

void Page::ShredRecord(const void* ptr)
{
    PLATFORM_CRITICAL_SECTION();
    const Page* p = FromPtrInline(ptr);

    if (p->recordSize)
    {
        // easy for fixed records
        Flash::ShredDouble(ptr);
        return;
    }

    // for variable records, shredding a record will be hopefully performed without
    // interruption - if it is interrupted, the record may still appear valid
    // but it can be partially shredded from the end
    // this is still better than starting from the beginning, which would
    // corrupt the record continuity (shredding the first dword would make the
    // payload of the record be treated as the header of the next record)
    uint32_t totalLength = VarGetLen(ptr);
    ASSERT(totalLength != 0 && totalLength != ~0u);
    auto start = (const uint8_t*)ptr - 4;
    auto end = start + VarSkipLen(totalLength);
    if (end > endof(p->data))
    {
        // if the record was corrupted, just erase the rest of page...
        MYDBG("Erasing the rest of corrupted page from %p", start);
        end = endof(p->data);
    }

    for (auto shred = end - 8; shred >= start; shred -= 8)
    {
        Flash::ShredDouble(shred);
    }
}

#endif

/*!
 * Deletes all records with the specified key (i.e. firstWords)
 */
bool Page::Delete(ID page, uint32_t firstWord)
{
    PLATFORM_CRITICAL_SECTION();
    Span rec = FindUnorderedFirst(page, firstWord);

    if (!rec)
    {
        // no record exists
        return false;
    }

    // delete all matching records before notifying
    do
    {
        MYDBG("Deleting record: %08X", rec.Pointer());
        ShredRecord(rec);
    } while ((rec = FindUnorderedNext(rec, firstWord)));

    _manager.Notify(page);
    return true;
}

/*!
 * Moves all records from the old page to the new page
 */
bool Page::MoveRecords(const Page* p, size_t limit) const
{
    PLATFORM_CRITICAL_SECTION();
    ASSERT(p);
    ASSERT(p->id == id);
    const uint8_t* free = p->FindFree();

    if (!free)
    {
        return false;
    }

    const uint8_t* freeMax = endof(p->data);
    if (limit && free + limit < freeMax)
    {
        freeMax = free + limit;
    }

    const uint8_t* testFree = free;

    // first simulate moving the records and start only if they fit
    for (Span rec = FindForwardNextImpl(this, NULL, 0, NULL); rec; rec = FindForwardNextImpl(this, rec, 0, NULL))
    {
        if (p->recordSize)
        {
            // if the new page is fixed size, old records must also all be small enough
            if (testFree + p->recordSize > freeMax || rec.Length() > p->recordSize)
            {
                return false;
            }
            testFree += p->recordSize;
        }
        else
        {
            uint32_t requiredLength = RequiredAligned(rec.Length() + 4);
            if (testFree - 4 + requiredLength > freeMax)
            {
                return false;
            }
            // include the space for the length of the next record
            testFree += requiredLength;
        }
    }

    // records should fit, move them
    int moved = 0;
    bool success = true;

    for (Span rec = FindForwardNextImpl(this, NULL, 0, NULL); rec; rec = FindForwardNextImpl(this, rec, 0, NULL))
    {
        // free can point past the end of data if the last moved record filled the page exactly to the end
        if (free < endof(p->data))
        {
            auto span = Span(WriteImpl(free, rec.Element<uint32_t>(), rec.Pointer() + 4, rec.Length()));

            if (span)
            {
                // successful write
                ShredRecord(rec);
                free = span.Pointer<uint8_t>() + (p->recordSize ? p->recordSize : VarSkipLen(rec.Length()));
                continue;
            }
        }

        success = false;
        break;
    }

    if (moved)
    {
        MYDBG("Moved %d records from page %.4s-%d @ %08X to page %.4s-%d @ %08X", moved,
            &id, sequence, this,
            &p->id, p->sequence, p);
        _manager.Notify(id);
    }

    return success;
}

}
