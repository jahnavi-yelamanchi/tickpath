// OWNER: FlatBook and TreeBook against the book contract.

#include <gtest/gtest.h>

#include "tickpath/book/flat_book.hpp"
#include "tickpath/book/tree_book.hpp"

#include "book/book_contract_tests.hpp"

namespace tickpath::test {

using FastBooks = ::testing::Types<book::FlatBook, book::TreeBook>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fast, BookContract, FastBooks);

} // namespace tickpath::test
