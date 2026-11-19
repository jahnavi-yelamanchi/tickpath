// OWNER: FlatBook and TreeBook against the oracle under random operations.

#include <vector>

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "tickpath/book/flat_book.hpp"
#include "tickpath/book/tree_book.hpp"

#include "property/book_ops.hpp"

namespace tickpath::test {

RC_GTEST_PROP(FlatBookProp, MatchesOracle, ()) {
    check_against_oracle<book::FlatBook>(*rc::gen::container<std::vector<Op>>(gen_op()));
}

RC_GTEST_PROP(TreeBookProp, MatchesOracle, ()) {
    check_against_oracle<book::TreeBook>(*rc::gen::container<std::vector<Op>>(gen_op()));
}

} // namespace tickpath::test
