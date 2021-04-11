/*
 * Copyright (c) 2019 triaxis s.r.o.
 * Licensed under the MIT license. See LICENSE.txt file in the repository root
 * for full license information.
 *
 * nvram/tests/sanity/Storage.cpp
 */

#include <testrunner/TestCase.h>

#include <nvram/nvram.h>

using namespace nvram;

namespace
{

TEST_CASE("01 FixedStorage")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    struct Test { uint8_t a, b; };
    FixedStorage<Test> storage("TEST");

    auto t = storage.Add({ 1, 2 });
    AssertEqual(Span((Test){ 1, 2 }), Span(*t));
    t = storage.Add({ 3, 4 });
    AssertEqual(Span((Test){ 3, 4 }), Span(*t));

    auto unordered1 = storage.UnorderedFirst();
    auto unordered2 = storage.UnorderedNext(unordered1);

    auto newest1 = storage.NewestFirst();
    auto newest2 = storage.NewestNext(newest1);

    auto oldest1 = storage.OldestFirst();
    auto oldest2 = storage.OldestNext(oldest1);

    AssertNotEqual((const Test*)NULL, unordered2);
    AssertEqual((const Test*)NULL, storage.UnorderedNext(unordered2));
    AssertNotEqual((const Test*)NULL, newest2);
    AssertEqual((const Test*)NULL, storage.NewestNext(newest2));
    AssertNotEqual((const Test*)NULL, oldest2);
    AssertEqual((const Test*)NULL, storage.OldestNext(oldest2));

    AssertEqual(oldest1, newest2);
    AssertEqual(oldest2, newest1);
}

TEST_CASE("02 VariableStorage")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    VariableStorage storage("TEST");

    auto span = storage.Add(BYTES(1));
    AssertEqual(Span(BYTES(1)), span);
    span = storage.Add(BYTES(2, 3, 4));
    AssertEqual(Span(BYTES(2, 3, 4)), span);

    auto unordered1 = storage.UnorderedFirst();
    auto unordered2 = storage.UnorderedNext(unordered1);

    auto newest1 = storage.NewestFirst();
    auto newest2 = storage.NewestNext(newest1);

    auto oldest1 = storage.OldestFirst();
    auto oldest2 = storage.OldestNext(oldest1);

    AssertNotEqual((const void*)NULL, unordered2);
    AssertEqual((const void*)NULL, storage.UnorderedNext(unordered2));
    AssertNotEqual((const void*)NULL, newest2);
    AssertEqual((const void*)NULL, storage.NewestNext(newest2));
    AssertNotEqual((const void*)NULL, oldest2);
    AssertEqual((const void*)NULL, storage.OldestNext(oldest2));

    AssertEqual(oldest1, newest2);
    AssertEqual(oldest2, newest1);

    AssertEqual(1u, oldest1.Length());
    AssertEqual(3u, oldest2.Length());
}

TEST_CASE("03a FixedKeyStorage - Add")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    struct Test { uint8_t a, b; };
    FixedKeyStorage<Test> storage("TEST");

    auto t = storage.Add(1, { 1, 2 });
    AssertEqual(Span((Test){ 1, 2 }), Span(*t));
    t = storage.Add(2, { 3, 4 });
    AssertEqual(Span((Test){ 3, 4 }), Span(*t));
    t = storage.Add(1, { 5, 6 });
    AssertEqual(Span((Test){ 5, 6 }), Span(*t));
    t = storage.Add(2, { 7, 8 });
    AssertEqual(Span((Test){ 7, 8 }), Span(*t));

    for (ID key: WORDS(1, 2))
    {
        auto unordered1 = storage.UnorderedFirst(key);
        auto unordered2 = storage.UnorderedNext(unordered1);

        auto newest1 = storage.NewestFirst(key);
        auto newest2 = storage.NewestNext(newest1);

        auto oldest1 = storage.OldestFirst(key);
        auto oldest2 = storage.OldestNext(oldest1);

        AssertNotEqual((const void*)NULL, unordered2);
        AssertEqual((const void*)NULL, storage.UnorderedNext(unordered2));
        AssertNotEqual((const void*)NULL, newest2);
        AssertEqual((const void*)NULL, storage.NewestNext(newest2));
        AssertNotEqual((const void*)NULL, oldest2);
        AssertEqual((const void*)NULL, storage.OldestNext(oldest2));

        AssertEqual(oldest1, newest2);
        AssertEqual(oldest2, newest1);
    }

    // test enumerator
    ID id;
    const Test* unordered[] = { NULL, storage.UnorderedFirst(1), storage.UnorderedFirst(2) };

    for (auto t = storage.EnumerateUnorderedFirst(id); t; t = storage.EnumerateUnorderedNext(t, id))
    {
        AssertEqual(t, unordered[id]);
        unordered[id] = storage.UnorderedNext(unordered[id]);
    }

    AssertEqual((const void*)NULL, unordered[1]);
    AssertEqual((const void*)NULL, unordered[2]);
}

TEST_CASE("03b FixedKeyStorage - Replace")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    struct Test { uint8_t a, b; };
    FixedKeyStorage<Test> storage("TEST");

    auto t = storage.Add(1, { 1, 2 });
    AssertEqual(Span((Test){ 1, 2 }), Span(*t));
    t = storage.Add(2, { 3, 4 });
    AssertEqual(Span((Test){ 3, 4 }), Span(*t));
    t = storage.Add(1, { 5, 6 });
    AssertEqual(Span((Test){ 5, 6 }), Span(*t));
    t = storage.Add(2, { 7, 8 });
    AssertEqual(Span((Test){ 7, 8 }), Span(*t));
    t = storage.Replace(1, { 9, 10 });
    AssertEqual(Span((Test){ 9, 10 }), Span(*t));
    t = storage.Replace(2, { 11, 12 });
    AssertEqual(Span((Test){ 11, 12 }), Span(*t));

    for (ID key: WORDS(1, 2))
    {
        auto unordered1 = storage.UnorderedFirst(key);
        auto unordered2 = storage.UnorderedNext(unordered1);

        auto newest1 = storage.NewestFirst(key);
        auto newest2 = storage.NewestNext(newest1);

        auto oldest1 = storage.OldestFirst(key);
        auto oldest2 = storage.OldestNext(oldest1);

        AssertEqual((const void*)NULL, unordered2);
        AssertEqual((const void*)NULL, newest2);
        AssertEqual((const void*)NULL, oldest2);

        AssertEqual(oldest1, newest1);
        AssertEqual(unordered1, newest1);
    }

    // test enumerator
    ID id;
    const Test* unordered[] = { NULL, storage.UnorderedFirst(1), storage.UnorderedFirst(2) };

    for (auto t = storage.EnumerateUnorderedFirst(id); t; t = storage.EnumerateUnorderedNext(t, id))
    {
        AssertEqual(t, unordered[id]);
        unordered[id] = storage.UnorderedNext(unordered[id]);
    }

    AssertEqual((const void*)NULL, unordered[1]);
    AssertEqual((const void*)NULL, unordered[2]);
}

TEST_CASE("04a VariableKeyStorage - Add")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    VariableKeyStorage storage("TEST");

    auto span = storage.Add(1, BYTES(1, 2));
    AssertEqual(Span(BYTES(1, 2)), span);
    span = storage.Add(2, BYTES(3, 4));
    AssertEqual(Span(BYTES(3, 4)), span);
    span = storage.Add(1, WORDS(5, 6));
    AssertEqual(Span(WORDS(5, 6)), span);
    span = storage.Add(2, WORDS(7, 8));
    AssertEqual(Span(WORDS(7, 8)), span);

    for (ID key: WORDS(1, 2))
    {
        auto unordered1 = storage.UnorderedFirst(key);
        auto unordered2 = storage.UnorderedNext(unordered1);

        auto newest1 = storage.NewestFirst(key);
        auto newest2 = storage.NewestNext(newest1);

        auto oldest1 = storage.OldestFirst(key);
        auto oldest2 = storage.OldestNext(oldest1);

        AssertNotEqual((const void*)NULL, unordered2);
        AssertEqual((const void*)NULL, storage.UnorderedNext(unordered2));
        AssertNotEqual((const void*)NULL, newest2);
        AssertEqual((const void*)NULL, storage.NewestNext(newest2));
        AssertNotEqual((const void*)NULL, oldest2);
        AssertEqual((const void*)NULL, storage.OldestNext(oldest2));

        AssertEqual(oldest1, newest2);
        AssertEqual(oldest2, newest1);
    }

    // test enumerator
    ID id;
    Span unordered[] = { Span(), storage.UnorderedFirst(1), storage.UnorderedFirst(2) };

    for (auto t = storage.EnumerateUnorderedFirst(id); t; t = storage.EnumerateUnorderedNext(t, id))
    {
        AssertEqual(t, unordered[id]);
        unordered[id] = storage.UnorderedNext(unordered[id]);
    }

    AssertEqual((const void*)NULL, unordered[1]);
    AssertEqual((const void*)NULL, unordered[2]);
}

TEST_CASE("04b VariableKeyStorage - Replace")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    VariableKeyStorage storage("TEST");

    auto span = storage.Add(1, BYTES(1, 2));
    AssertEqual(Span(BYTES(1, 2)), span);
    span = storage.Add(2, BYTES(3, 4));
    AssertEqual(Span(BYTES(3, 4)), span);
    span = storage.Add(1, WORDS(5, 6));
    AssertEqual(Span(WORDS(5, 6)), span);
    span = storage.Add(2, WORDS(7, 8));
    AssertEqual(Span(WORDS(7, 8)), span);
    span = storage.Replace(1, BYTES(9, 10, 11, 12, 13));
    AssertEqual(Span(BYTES(9, 10, 11, 12, 13)), span);
    span = storage.Replace(2, BYTES(15, 16, 17, 18, 19, 20));
    AssertEqual(Span(BYTES(15, 16, 17, 18, 19, 20)), span);

    for (ID key: WORDS(1, 2))
    {
        auto unordered1 = storage.UnorderedFirst(key);
        auto unordered2 = storage.UnorderedNext(unordered1);

        auto newest1 = storage.NewestFirst(key);
        auto newest2 = storage.NewestNext(newest1);

        auto oldest1 = storage.OldestFirst(key);
        auto oldest2 = storage.OldestNext(oldest1);

        AssertEqual((const void*)NULL, unordered2);
        AssertEqual((const void*)NULL, newest2);
        AssertEqual((const void*)NULL, oldest2);

        AssertEqual(oldest1, newest1);
        AssertEqual(unordered1, newest1);
    }

    // test enumerator
    ID id;
    Span unordered[] = { Span(), storage.UnorderedFirst(1), storage.UnorderedFirst(2) };

    for (auto t = storage.EnumerateUnorderedFirst(id); t; t = storage.EnumerateUnorderedNext(t, id))
    {
        AssertEqual(t, unordered[id]);
        unordered[id] = storage.UnorderedNext(unordered[id]);
    }

    AssertEqual((const void*)NULL, unordered[1]);
    AssertEqual((const void*)NULL, unordered[2]);
}

TEST_CASE("05 FixedUniqueKeyStorage")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    struct Test { int a, b; };
    FixedUniqueKeyStorage<Test> storage("TEST");

    AssertEqual((const Test*)NULL, storage.Get(1));
    auto t = storage.Set(1, (Test){ 1, 2 });
    AssertEqual(Span((Test){ 1, 2 }), Span(*t));
    AssertEqual(t, storage.Get(1));
    t = storage.Set(1, (Test) { 2, 3 });
    AssertEqual(Span((Test){ 2, 3 }), Span(*t));
    AssertEqual(t, storage.Get(1));
};

TEST_CASE("06 VariableUniqueKeyStorage")
{
    nvram::Initialize(Span(), nvram::InitFlags::Reset);

    VariableUniqueKeyStorage storage("TEST");

    AssertEqual((const void*)NULL, storage.Get(1));
    auto span = storage.Set(1, BYTES(1, 2));
    AssertEqual(Span(BYTES(1, 2)), span);
    AssertEqual(span.Pointer(), storage.Get(1).Pointer());
    span = storage.Set(1, WORDS(2, 3));
    AssertEqual(Span(WORDS(2, 3)), span);
    AssertEqual(span.Pointer(), storage.Get(1).Pointer());
};

}

