// The differential harness itself, run oracle-vs-oracle (must pass), plus a
// deliberately broken book to show the harness catches divergence.

#include <gtest/gtest.h>

#include "tickpath/book/reference_book.hpp"

#include "differential/differential_tests.hpp"

namespace tickpath::test {

INSTANTIATE_TYPED_TEST_SUITE_P(OracleVsOracle, Differential, ::testing::Types<book::ReferenceBook>);

namespace {

/// Oracle that forgets time priority: re-adding at the front of the queue on
/// replace is invisible to level aggregates but must be caught.
class PriorityBugBook : public book::ReferenceBook {
public:
    explicit PriorityBugBook(const book::BookConfig& cfg)
        : ReferenceBook(cfg) {}
    book::Status replace(Locate l, book::OrderId old_id, book::OrderId new_id, book::Qty q,
                         Price p) noexcept {
        const auto o = find(old_id);
        const auto st = ReferenceBook::replace(l, old_id, new_id, q, p);
        if (st != book::Status::Ok || !o || o->price != p) {
            return st;
        }
        // Move the new order ahead of its level by re-adding everyone else.
        std::vector<Order> others;
        for_each_order(l, o->side, p, [&](const Order& x) {
            if (x.id != new_id) {
                others.push_back(x);
            }
        });
        for (const auto& x : others) {
            static_cast<void>(ReferenceBook::remove(l, x.id));
            static_cast<void>(ReferenceBook::add(l, x.id, x.side, x.qty, x.price));
        }
        return st;
    }
};

/// Oracle that ignores locate mismatch on delete.
class LocateBugBook : public book::ReferenceBook {
public:
    explicit LocateBugBook(const book::BookConfig& cfg)
        : ReferenceBook(cfg) {}
    book::Status remove(Locate /*l*/, book::OrderId id) noexcept {
        const auto o = find(id);
        return o ? ReferenceBook::remove(o->locate, id) : book::Status::UnknownOrder;
    }
};

} // namespace

TEST(DifferentialHarness, CatchesPriorityBug) {
    const auto d = run_differential<PriorityBugBook>(sim::generate_edge_cases());
    ASSERT_TRUE(d.has_value());
    EXPECT_NE(d->what.find("queue"), std::string::npos) << d->what;
}

TEST(DifferentialHarness, CatchesStatusBug) {
    const auto d = run_differential<LocateBugBook>(sim::generate_contract_violations());
    ASSERT_TRUE(d.has_value());
    EXPECT_NE(d->what.find("status"), std::string::npos) << d->what;
}

} // namespace tickpath::test
