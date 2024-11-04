/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/Page.Search.cpp
 *
 * NVRAM page traversal and record search
 */

#include <nvram/Page.h>

namespace nvram
{

/**************************************************/
/**************** UNORDERED SEARCH ****************/
/**************************************************/


/*!
 * Returns the first matching record in no specific order.
 */
Span::packed_t Page::FindUnorderedFirstImpl(ID page, uint32_t firstWord)
{
    const Page* p = First(page);
    if (!p)
        return Span();
    return FindForwardNextImpl(p, NULL, firstWord, Page::UnorderedNextImpl);
}

/*!
 * Returns the next matching record in no specific order.
 *
 * Search continues *after* the specified record (rec).
 */
Span::packed_t Page::FindUnorderedNextImpl(const uint8_t* rec, uint32_t firstWord)
{
    return FindForwardNextImpl(FromPtrInline(rec), rec, firstWord, Page::UnorderedNextImpl);
}

/*!
 * Returns the next matching record in no specific order.
 *
 * Search starts *after* the specified record, or at the start of the page if rec is NULL
 */
Span::packed_t Page::FindForwardNextImpl(const Page* p, const uint8_t* rec, uint32_t firstWord, const Page* (*nextPage)(const Page*))
{
    do
    {
        const uint8_t* pe = p->data + PagePayload;

        if (uint32_t recordSize = p->recordSize)
        {
            // fixed records
            if (rec)
                rec += recordSize;
            else
                rec = p->data;

            for (; rec + recordSize < pe; rec += recordSize)
            {
                uint32_t first = FirstWord(rec);
                if (first == 0)
                {
                    continue;
                }
                else if (first != ~0u)
                {
                    if (firstWord == 0 || first == firstWord)
                    {
                        return Span(rec, recordSize);
                    }
                }
            }
        }
        else
        {
            // variable records
            if (rec)
                rec = VarNext(rec);
            else
                rec = p->data + 4;

            uint32_t len, first;

            for (; rec < pe; rec += VarSkipLen(len))
            {
                len = VarGetLen(rec);
                if (len == 0)
                {
                    continue;
                }
                else if (len != ~0u && (first = FirstWord(rec)) != 0)
                {
                    if (firstWord == 0 || first == firstWord)
                    {
                        return Span(rec, len);
                    }
                }
            }
        }

        // try the next page
        rec = NULL;
    } while ((p = nextPage ? nextPage(p) : NULL));

    return Span();
}

/*!
 * Returns the first valid record on the specified page
 */
Span::packed_t Page::FirstRecordImpl(const Page* p)
{
    return FindForwardNextImpl(p, NULL, 0, NULL);
}

/*!
 * Returns the last valid record on the specified page
 */
Span::packed_t Page::LastRecordImpl(const Page* p)
{
    return FindNewestNextImpl(p, NULL, 0, NULL);
}

/*!
 * Returns the next valid record on the same page
 */
Span::packed_t Page::NextRecordImpl(const uint8_t* record)
{
    return FindForwardNextImpl(FromPtrInline(record), record, 0, NULL);
}

/**************************************************/
/*************** NEW-TO-OLD SEARCH ****************/
/**************************************************/


/*!
 * Returns the newest matching record.
 */
Span::packed_t Page::FindNewestFirstImpl(ID page, uint32_t firstWord)
{
    const Page* p = NewestFirst(page);
    if (!p)
        return Span();
    return FindNewestNextImpl(p, NULL, firstWord, Page::NewestNextImpl);
}

/*!
 * Returns the newest matching record, before the specified one. Stops searching when it encounters the stop record.
 */
Span::packed_t Page::FindNewestNextImpl(const uint8_t* stop, uint32_t firstWord)
{
    return FindNewestNextImpl(FromPtrInline(stop), stop, firstWord, Page::NewestNextImpl);
}

/*!
 * Returns the newest matching record, starting on the specified page. Stops searching when it encounters the stop record.
 */
Span::packed_t Page::FindNewestNextImpl(const Page* p, const uint8_t* stop, uint32_t firstWord, const Page* (*nextPage)(const Page*))
{
    const uint8_t* found = NULL;

    do
    {
        const uint8_t* pe = p->data + PagePayload;

        if (uint32_t recordSize = p->recordSize)
        {
            // fixed records
            const uint8_t* rec = p->data;

            for (; rec + recordSize < pe && rec != stop; rec += recordSize)
            {
                uint32_t first = FirstWord(rec);
                if (first == 0)
                {
                    continue;
                }
                else if (first != ~0u)
                {
                    if (firstWord == 0 || first == firstWord)
                    {
                        found = rec;
                    }
                }
            }

            if (found)
                return Span(found, recordSize);
        }
        else
        {
            // variable records
            const uint8_t* rec = p->data + 4;
            uint32_t len, first;

            for (; rec < pe && rec != stop; rec += VarSkipLen(len))
            {
                len = VarGetLen(rec);
                if (len == 0)
                {
                    continue;
                }
                else if (len != ~0u && (first = FirstWord(rec)) != 0)
                {
                    if (firstWord == 0 || first == firstWord)
                    {
                        found = rec;
                    }
                }
            }

            if (found)
                return Span(found, VarGetLen(found));
        }

        // try the next page
    } while ((p = nextPage ? nextPage(p) : NULL));

    return Span();
}



/**************************************************/
/*************** OLD-TO-NEW SEARCH ****************/
/**************************************************/


/*!
 * Returns the oldest matching record on pages with specified ID
 */
Span::packed_t Page::FindOldestFirstImpl(ID page, uint32_t firstWord)
{
    const Page* p = OldestFirst(page);
    if (!p)
        return Span();
    return FindForwardNextImpl(p, NULL, firstWord, Page::OldestNextImpl);
}

/*!
 * Returns the oldest matching record, starting after the specified record.
 */
Span::packed_t Page::FindOldestNextImpl(const uint8_t* rec, uint32_t firstWord)
{
    return FindForwardNextImpl(FromPtrInline(rec), rec, firstWord, Page::OldestNextImpl);
}

}
