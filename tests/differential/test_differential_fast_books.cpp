// OWNER: FlatBook and TreeBook against the oracle, after every message.

#include <gtest/gtest.h>

#include "tickpath/book/flat_book.hpp"
#include "tickpath/book/tree_book.hpp"

#include "differential/differential_tests.hpp"

namespace tickpath::test {

using FastBooks = ::testing::Types<book::FlatBook, book::TreeBook>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fast, Differential, FastBooks);

} // namespace tickpath::test
