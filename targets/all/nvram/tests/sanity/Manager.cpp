/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/tests/sanity/Manager.cpp
 */

#include <testrunner/TestCase.h>

#include <nvram/nvram.h>

using namespace nvram;

namespace
{

TEST_CASE("01 Init Clean")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    auto endTime = kernel::Scheduler::Main().Run();
    AssertEqual(0u, endTime);
}

TEST_CASE("02 Init Erase Random Data")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);    // just erase

    for (auto& b: Blocks())
    {
        Flash::WriteWord(&b, 42);
    }

    // let background tasks complete before reinitializing
    auto endTime = kernel::Scheduler::Main().Run();
    AssertEqual(0u, endTime);

    nvram::Initialize(Span());  // second init without erase

    endTime = kernel::Scheduler::Main().Run();
    AssertNotEqual(0u, endTime);

    for (auto& b : Blocks())
    {
        AssertEqual(true, b.IsEmpty());
    }
}

TEST_CASE("03 Init Erase Free Blocks")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    for (auto& b: Blocks())
    {
        for (UNUSED auto& p: b)
        {
            Page::New("TEST");
        }
    }

    for (auto& b: Blocks())
    {
        for (auto& p: b)
        {
            Flash::ShredWord(&p);
        }
    }

    // let background tasks complete before reinitializing
    auto endTime = kernel::Scheduler::Main().Run();
    AssertEqual(0u, endTime);

    nvram::Initialize(Span());

    endTime = kernel::Scheduler::Main().Run();
    AssertNotEqual(0u, endTime);

    for (auto& b : Blocks())
    {
        AssertEqual(2u, b.Generation());
    }
}

TEST_CASE("04 Collect Oldest")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);
    nvram::RegisterCollector("TEST", 1, CollectorDiscardOldest);

    for (auto& b: Blocks())
    {
        for (UNUSED auto& p: b)
        {
            Page::New("TEST");
        }
    }

    // cannot allocate page
    AssertEqual((const Page*)NULL, Page::New("TEST"));

    // let background tasks complete before reinitializing
    auto endTime = kernel::Scheduler::Main().Run();
    AssertNotEqual(0u, endTime);

    // can allocate page
    AssertNotEqual((const Page*)NULL, Page::New("TEST"));

    // let background tasks complete before reinitializing
    endTime = kernel::Scheduler::Main().Run();
    AssertNotEqual(0u, endTime);
}

}
