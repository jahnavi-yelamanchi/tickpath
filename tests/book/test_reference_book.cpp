// The oracle against its own contract. Must pass: every other book is
// compared to it.

#include <gtest/gtest.h>

#include "tickpath/book/reference_book.hpp"
#include "tickpath/book/state_hash.hpp"

#include "book/book_contract_tests.hpp"

namespace tickpath::test {

INSTANTIATE_TYPED_TEST_SUITE_P(Reference, BookContract, ::testing::Types<book::ReferenceBook>);

TEST(ReferenceBookHash, EqualStateEqualHash) {
    book::ReferenceBook a;
    book::ReferenceBook b;
    ASSERT_EQ(a.add(1, 1, Side::Buy, 100, 100), book::Status::Ok);
    ASSERT_EQ(a.add(1, 2, Side::Buy, 100, 100), book::Status::Ok);
    // Same final state through a different path.
    ASSERT_EQ(b.add(1, 1, Side::Buy, 300, 100), book::Status::Ok);
    ASSERT_EQ(b.add(1, 2, Side::Buy, 100, 100), book::Status::Ok);
    ASSERT_EQ(b.cancel(1, 1, 200), book::Status::Ok);
    EXPECT_EQ(book::state_hash(a), book::state_hash(b));
    EXPECT_EQ(book::symbol_hash(a, 1), book::symbol_hash(b, 1));
}

TEST(ReferenceBookHash, PriorityChangesHash) {
    book::ReferenceBook a;
    book::ReferenceBook b;
    ASSERT_EQ(a.add(1, 1, Side::Buy, 100, 100), book::Status::Ok);
    ASSERT_EQ(a.add(1, 2, Side::Buy, 100, 100), book::Status::Ok);
    ASSERT_EQ(b.add(1, 2, Side::Buy, 100, 100), book::Status::Ok);
    ASSERT_EQ(b.add(1, 1, Side::Buy, 100, 100), book::Status::Ok);
    EXPECT_NE(book::symbol_hash(a, 1), book::symbol_hash(b, 1));
}

} // namespace tickpath::test
