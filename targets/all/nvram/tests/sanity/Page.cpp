/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/tests/sanity/Page.cpp
 */

#include <testrunner/TestCase.h>

#include <nvram/nvram.h>

#include <vector>

using namespace nvram;

namespace
{

TEST_CASE("01 Page Alloc")
{
    nvram::Initialize(Span(), true);

    auto page = Page::New("TEST", 0);

    AssertNotEqual((const Page*)NULL, page);
    AssertEqual(page, Page::First("TEST"));
}

TEST_CASE("02 Page Max Alloc")
{
    nvram::Initialize(Span(), true);

    size_t cnt = Flash::GetRange().Length() / Flash::PageSize * PagesPerBlock;

    for (size_t i = 0; i < cnt; i++)
    {
        auto p = Page::New("TEST", 0);
        AssertNotEqual((const Page*)NULL, p);
        AssertEqual(i + 1, p->Sequence());
    }

    AssertEqual((const Page*)NULL, Page::New("TEST", 0));
}

static int ScatterFill()
{
    nvram::Initialize(Span(), true);

    int last = 0;
    while (auto p = Page::New("TEST", 0))
    {
        last = p->Sequence();
        // allocate other pages to shuffle the sequence around a bit
        Page::New("FILL", 0);
        Page::New("FILL", 0);
    }

    return last;
}

TEST_CASE("03 Scan Random")
{
    std::vector<bool> found;

    ScatterFill();

    for (auto p = Page::First("TEST"); p; p = p->Next())
    {
        DBGL("Found page TEST-%d @ %p", p->Sequence(), p);
        found.reserve(p->Sequence() - 1);
        found[p->Sequence() - 1] = true;
    }

    for (bool b: found)
    {
        Assert(b);
    }
}

TEST_CASE("04 Scan Old To New")
{
    ScatterFill();

    int i = 1;
    for (auto p = Page::OldestFirst("TEST"); p; p = p->OldestNext())
    {
        AssertEqual(i++, p->Sequence());
    }
}

TEST_CASE("05 Scan New To Old")
{
    int i = ScatterFill();

    for (auto p = Page::NewestFirst("TEST"); p; p = p->NewestNext())
    {
        AssertEqual(i--, p->Sequence());
    }
}

}
