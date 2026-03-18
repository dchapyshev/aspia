//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/shared_pointer.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace base {

namespace {

// Helper to track construction/destruction.
struct LifetimeTracker
{
    static int alive_count;

    int value;

    explicit LifetimeTracker(int v = 0) : value(v) { ++alive_count; }
    ~LifetimeTracker() { --alive_count; }

    // Non-copyable to make sure SharedPointer doesn't copy the object.
    LifetimeTracker(const LifetimeTracker&) = delete;
    LifetimeTracker& operator=(const LifetimeTracker&) = delete;
};

int LifetimeTracker::alive_count = 0;

} // namespace

// ============================================================================
// Default constructor
// ============================================================================

TEST(shared_pointer_test, default_constructor)
{
    SharedPointer<int> sp;
    EXPECT_TRUE(sp.isEmpty());
    EXPECT_FALSE(sp);
    EXPECT_EQ(sp.get(), nullptr);
    EXPECT_EQ(sp.useCount(), 0);
    EXPECT_FALSE(sp.isUnique());
}

// ============================================================================
// Constructor from raw pointer
// ============================================================================

TEST(shared_pointer_test, construct_from_pointer)
{
    SharedPointer<int> sp(new int(42));
    EXPECT_FALSE(sp.isEmpty());
    EXPECT_TRUE(sp);
    EXPECT_NE(sp.get(), nullptr);
    EXPECT_EQ(*sp, 42);
    EXPECT_EQ(sp.useCount(), 1);
    EXPECT_TRUE(sp.isUnique());
}

TEST(shared_pointer_test, construct_deletes_on_destruction)
{
    LifetimeTracker::alive_count = 0;
    {
        SharedPointer<LifetimeTracker> sp(new LifetimeTracker(10));
        EXPECT_EQ(LifetimeTracker::alive_count, 1);
        EXPECT_EQ(sp->value, 10);
    }
    EXPECT_EQ(LifetimeTracker::alive_count, 0);
}

// ============================================================================
// Copy constructor
// ============================================================================

TEST(shared_pointer_test, copy_constructor)
{
    SharedPointer<int> sp1(new int(100));
    SharedPointer<int> sp2(sp1);

    EXPECT_EQ(sp1.get(), sp2.get());
    EXPECT_EQ(*sp1, 100);
    EXPECT_EQ(*sp2, 100);
    EXPECT_EQ(sp1.useCount(), 2);
    EXPECT_EQ(sp2.useCount(), 2);
    EXPECT_FALSE(sp1.isUnique());
    EXPECT_FALSE(sp2.isUnique());
}

TEST(shared_pointer_test, copy_constructor_shares_lifetime)
{
    LifetimeTracker::alive_count = 0;
    SharedPointer<LifetimeTracker> sp2;
    {
        SharedPointer<LifetimeTracker> sp1(new LifetimeTracker(5));
        sp2 = sp1;
        EXPECT_EQ(LifetimeTracker::alive_count, 1);
        EXPECT_EQ(sp1.useCount(), 2);
    }
    // sp1 destroyed, but sp2 still holds the object.
    EXPECT_EQ(LifetimeTracker::alive_count, 1);
    EXPECT_EQ(sp2.useCount(), 1);
    EXPECT_EQ(sp2->value, 5);
}

TEST(shared_pointer_test, copy_constructor_from_empty)
{
    SharedPointer<int> sp1;
    SharedPointer<int> sp2(sp1);

    EXPECT_TRUE(sp1.isEmpty());
    EXPECT_TRUE(sp2.isEmpty());
    EXPECT_EQ(sp1.useCount(), 0);
    EXPECT_EQ(sp2.useCount(), 0);
}

// ============================================================================
// Copy assignment
// ============================================================================

TEST(shared_pointer_test, copy_assignment)
{
    SharedPointer<int> sp1(new int(1));
    SharedPointer<int> sp2(new int(2));

    sp2 = sp1;

    EXPECT_EQ(*sp2, 1);
    EXPECT_EQ(sp1.get(), sp2.get());
    EXPECT_EQ(sp1.useCount(), 2);
}

TEST(shared_pointer_test, copy_assignment_releases_old)
{
    LifetimeTracker::alive_count = 0;
    SharedPointer<LifetimeTracker> sp1(new LifetimeTracker(1));
    SharedPointer<LifetimeTracker> sp2(new LifetimeTracker(2));

    EXPECT_EQ(LifetimeTracker::alive_count, 2);

    sp2 = sp1; // Object 2 should be deleted.
    EXPECT_EQ(LifetimeTracker::alive_count, 1);
    EXPECT_EQ(sp2->value, 1);
}

TEST(shared_pointer_test, self_assignment)
{
    SharedPointer<int> sp(new int(99));
    sp = sp;
    EXPECT_EQ(*sp, 99);
    EXPECT_EQ(sp.useCount(), 1);
}

// ============================================================================
// Move constructor
// ============================================================================

TEST(shared_pointer_test, move_constructor)
{
    SharedPointer<int> sp1(new int(77));
    int* raw = sp1.get();

    SharedPointer<int> sp2(std::move(sp1));

    EXPECT_TRUE(sp1.isEmpty());
    EXPECT_EQ(sp1.useCount(), 0);

    EXPECT_EQ(sp2.get(), raw);
    EXPECT_EQ(*sp2, 77);
    EXPECT_EQ(sp2.useCount(), 1);
}

TEST(shared_pointer_test, move_constructor_no_extra_delete)
{
    LifetimeTracker::alive_count = 0;
    {
        SharedPointer<LifetimeTracker> sp1(new LifetimeTracker(3));
        SharedPointer<LifetimeTracker> sp2(std::move(sp1));
        EXPECT_EQ(LifetimeTracker::alive_count, 1);
    }
    EXPECT_EQ(LifetimeTracker::alive_count, 0);
}

// ============================================================================
// Move assignment
// ============================================================================

TEST(shared_pointer_test, move_assignment)
{
    SharedPointer<int> sp1(new int(11));
    SharedPointer<int> sp2(new int(22));
    int* raw = sp1.get();

    sp2 = std::move(sp1);

    EXPECT_TRUE(sp1.isEmpty());
    EXPECT_EQ(sp2.get(), raw);
    EXPECT_EQ(*sp2, 11);
    EXPECT_EQ(sp2.useCount(), 1);
}

TEST(shared_pointer_test, move_assignment_releases_old)
{
    LifetimeTracker::alive_count = 0;
    SharedPointer<LifetimeTracker> sp1(new LifetimeTracker(1));
    SharedPointer<LifetimeTracker> sp2(new LifetimeTracker(2));

    EXPECT_EQ(LifetimeTracker::alive_count, 2);

    sp2 = std::move(sp1); // Object 2 deleted, object 1 transferred.
    EXPECT_EQ(LifetimeTracker::alive_count, 1);
    EXPECT_EQ(sp2->value, 1);
}

TEST(shared_pointer_test, self_move_assignment)
{
    SharedPointer<int> sp(new int(55));
    sp = std::move(sp);
    EXPECT_EQ(*sp, 55);
    EXPECT_EQ(sp.useCount(), 1);
}

// ============================================================================
// useCount / isUnique / isEmpty
// ============================================================================

TEST(shared_pointer_test, use_count_grows_and_shrinks)
{
    SharedPointer<int> sp1(new int(0));
    EXPECT_EQ(sp1.useCount(), 1);

    SharedPointer<int> sp2(sp1);
    EXPECT_EQ(sp1.useCount(), 2);

    {
        SharedPointer<int> sp3(sp1);
        EXPECT_EQ(sp1.useCount(), 3);
    }

    EXPECT_EQ(sp1.useCount(), 2);
}

// ============================================================================
// reset
// ============================================================================

TEST(shared_pointer_test, reset_to_nullptr)
{
    LifetimeTracker::alive_count = 0;
    SharedPointer<LifetimeTracker> sp(new LifetimeTracker(7));
    sp.reset();

    EXPECT_TRUE(sp.isEmpty());
    EXPECT_EQ(sp.useCount(), 0);
    EXPECT_EQ(sp.get(), nullptr);
    EXPECT_EQ(LifetimeTracker::alive_count, 0);
}

TEST(shared_pointer_test, reset_to_new_pointer)
{
    LifetimeTracker::alive_count = 0;
    SharedPointer<LifetimeTracker> sp(new LifetimeTracker(1));
    sp.reset(new LifetimeTracker(2));

    EXPECT_EQ(LifetimeTracker::alive_count, 1);
    EXPECT_EQ(sp->value, 2);
    EXPECT_EQ(sp.useCount(), 1);
}

TEST(shared_pointer_test, reset_shared_does_not_delete_for_others)
{
    LifetimeTracker::alive_count = 0;
    SharedPointer<LifetimeTracker> sp1(new LifetimeTracker(10));
    SharedPointer<LifetimeTracker> sp2(sp1);

    EXPECT_EQ(sp1.useCount(), 2);

    sp1.reset(); // sp1 detaches, sp2 still holds the object.
    EXPECT_TRUE(sp1.isEmpty());
    EXPECT_EQ(LifetimeTracker::alive_count, 1);
    EXPECT_EQ(sp2.useCount(), 1);
    EXPECT_EQ(sp2->value, 10);
}

TEST(shared_pointer_test, reset_empty_to_nullptr)
{
    SharedPointer<int> sp;
    sp.reset(); // Should not crash.
    EXPECT_TRUE(sp.isEmpty());
}

// ============================================================================
// swap
// ============================================================================

TEST(shared_pointer_test, swap_two_pointers)
{
    SharedPointer<int> sp1(new int(111));
    SharedPointer<int> sp2(new int(222));

    int* raw1 = sp1.get();
    int* raw2 = sp2.get();

    sp1.swap(sp2);

    EXPECT_EQ(sp1.get(), raw2);
    EXPECT_EQ(sp2.get(), raw1);
    EXPECT_EQ(*sp1, 222);
    EXPECT_EQ(*sp2, 111);
}

TEST(shared_pointer_test, swap_with_empty)
{
    SharedPointer<int> sp1(new int(333));
    SharedPointer<int> sp2;

    sp1.swap(sp2);

    EXPECT_TRUE(sp1.isEmpty());
    EXPECT_FALSE(sp2.isEmpty());
    EXPECT_EQ(*sp2, 333);
}

TEST(shared_pointer_test, swap_preserves_ref_count)
{
    SharedPointer<int> sp1(new int(1));
    SharedPointer<int> sp1_copy(sp1); // ref_count = 2
    SharedPointer<int> sp2(new int(2)); // ref_count = 1

    sp1.swap(sp2);

    // sp1 now points to old sp2's object (ref_count 1).
    EXPECT_EQ(sp1.useCount(), 1);
    // sp2 now points to old sp1's object (ref_count 2, shared with sp1_copy).
    EXPECT_EQ(sp2.useCount(), 2);
    EXPECT_EQ(sp1_copy.useCount(), 2);
    EXPECT_EQ(sp2.get(), sp1_copy.get());
}

// ============================================================================
// Operators: *, ->, bool
// ============================================================================

TEST(shared_pointer_test, dereference_operator)
{
    SharedPointer<std::string> sp(new std::string("hello"));
    EXPECT_EQ(*sp, "hello");
}

TEST(shared_pointer_test, arrow_operator)
{
    SharedPointer<std::string> sp(new std::string("world"));
    EXPECT_EQ(sp->size(), 5u);
}

TEST(shared_pointer_test, bool_conversion)
{
    SharedPointer<int> empty;
    SharedPointer<int> non_empty(new int(1));

    EXPECT_FALSE(empty);
    EXPECT_TRUE(non_empty);

    if (non_empty)
    {
        SUCCEED();
    }
    else
    {
        FAIL() << "non-empty SharedPointer should convert to true";
    }
}

// ============================================================================
// Multiple copies: ref count chain
// ============================================================================

TEST(shared_pointer_test, multiple_copies_ref_count)
{
    LifetimeTracker::alive_count = 0;
    {
        SharedPointer<LifetimeTracker> sp1(new LifetimeTracker(42));
        SharedPointer<LifetimeTracker> sp2(sp1);
        SharedPointer<LifetimeTracker> sp3(sp2);
        SharedPointer<LifetimeTracker> sp4 = sp3;

        EXPECT_EQ(sp1.useCount(), 4);
        EXPECT_EQ(LifetimeTracker::alive_count, 1);

        sp2.reset();
        EXPECT_EQ(sp1.useCount(), 3);

        sp3 = SharedPointer<LifetimeTracker>();
        EXPECT_EQ(sp1.useCount(), 2);
    }
    EXPECT_EQ(LifetimeTracker::alive_count, 0);
}

} // namespace base
