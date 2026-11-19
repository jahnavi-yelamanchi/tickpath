// Oracle invariants under random operations, valid and invalid (must pass).

#include <vector>

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "property/book_ops.hpp"

namespace tickpath::test {

RC_GTEST_PROP(OracleProp, InvariantsHoldAfterEveryOp, ()) {
    const auto ops = *rc::gen::container<std::vector<Op>>(gen_op());
    book::ReferenceBook b;
    for (const auto& op : ops) {
        const auto before = b.order_count();
        const auto st = apply_op(b, op);
        if (st != book::Status::Ok && st != book::Status::Overfill) {
            RC_ASSERT(b.order_count() == before);
        }
        check_invariants(b);
    }
}

RC_GTEST_PROP(OracleProp, AddThenRemoveRestoresState, ()) {
    const auto ops = *rc::gen::container<std::vector<Op>>(gen_op());
    book::ReferenceBook b;
    for (const auto& op : ops) {
        static_cast<void>(apply_op(b, op));
    }
    const auto before = snapshot(b, 1);
    const auto id = *rc::gen::inRange<book::OrderId>(1'000, 2'000);
    RC_ASSERT(b.add(1, id, *rc::gen::element(Side::Buy, Side::Sell),
                    *rc::gen::inRange<book::Qty>(1, 500),
                    *rc::gen::inRange<Price>(99'000, 101'000)) == book::Status::Ok);
    RC_ASSERT(b.remove(1, id) == book::Status::Ok);
    RC_ASSERT(snapshot(b, 1) == before);
}

RC_GTEST_PROP(OracleProp, OracleAgreesWithItself, ()) {
    check_against_oracle<book::ReferenceBook>(*rc::gen::container<std::vector<Op>>(gen_op()));
}

} // namespace tickpath::test
