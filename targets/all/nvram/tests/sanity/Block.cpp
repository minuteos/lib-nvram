/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/tests/sanity/Block.cpp
 */

#include <testrunner/TestCase.h>

#include <nvram/nvram.h>

using namespace nvram;

namespace
{

TEST_CASE("01 Init")
{
    nvram::Initialize(Span(), true);

    for (UNUSED auto blk: Block::Enumerate())
    {
        // there should be no blocks
        Assert(false);
    }
}

TEST_CASE("02 Block Alloc")
{
    nvram::Initialize(Span(), true);

    auto blk = Block::New();

    AssertNotEqual((const Block*)NULL, blk);

    int cnt = 0;
    for (auto& b: Block::Enumerate())
    {
        cnt++;
        AssertEqual(&b, blk);
    }

    AssertEqual(1, cnt);
}

TEST_CASE("03 Max Alloc")
{
    nvram::Initialize(Span(), true);

    size_t cnt = Flash::GetRange().Length() / Flash::PageSize;

    for (size_t i = 0; i < cnt; i++)
    {
        AssertNotEqual((const Block*)NULL, Block::New());
    }

    AssertEqual((const Block*)NULL, Block::New());

    for (UNUSED auto& b: Block::Enumerate())
    {
        cnt--;
    }

    AssertEqual(0u, cnt);
}

}
