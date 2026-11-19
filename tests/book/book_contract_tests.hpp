#pragma once

// Typed contract tests for any OrderBook (docs/BOOK_CONTRACT.md).
// Instantiate with INSTANTIATE_TYPED_TEST_SUITE_P(Name, BookContract, Types).

#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "tickpath/book/concept.hpp"
#include "tickpath/book/feed_adapter.hpp"
#include "tickpath/book/state_hash.hpp"
#include "tickpath/itch/messages.hpp"
#include "tickpath/sim/generator.hpp"

#include "support/book_snapshot.hpp"

namespace tickpath::test {

using book::BookConfig;
using book::OrderId;
using book::Qty;
using book::Status;

inline constexpr BookConfig contract_config{.max_orders = 1U << 16U, .max_levels = 1U << 16U};

template <class B>
class BookContract : public ::testing::Test {
protected:
    B book{contract_config};

    // Asserts that `op` returns `expected` and leaves the symbol unchanged.
    template <class Op>
    void expect_noop(Locate l, Status expected, Op op) {
        const auto before = snapshot(book, l);
        const auto count = book.order_count();
        EXPECT_EQ(op(), expected);
        EXPECT_EQ(snapshot(book, l), before);
        EXPECT_EQ(book.order_count(), count);
    }
};

TYPED_TEST_SUITE_P(BookContract);

constexpr Locate L1 = 1;
constexpr Locate L2 = 2;
constexpr Price P = 100'000;

// ---- Adds and ordering -------------------------------------------------------

TYPED_TEST_P(BookContract, AddCreatesLevelAndOrder) {
    auto& b = this->book;
    EXPECT_EQ(b.order_count(), 0U);
    EXPECT_FALSE(b.best(L1, Side::Buy));
    EXPECT_FALSE(b.find(1));
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    EXPECT_EQ(b.order_count(), 1U);
    EXPECT_EQ(b.best(L1, Side::Buy), (Level{.price = P, .qty = 100, .orders = 1}));
    EXPECT_FALSE(b.best(L1, Side::Sell));
    EXPECT_EQ(b.find(1), (Order{.id = 1, .locate = L1, .side = Side::Buy, .price = P, .qty = 100}));
    EXPECT_FALSE(b.best(L2, Side::Buy)) << "symbols are independent";
}

TYPED_TEST_P(BookContract, LevelsSortedBestFirst) {
    auto& b = this->book;
    const std::vector<Price> prices{P, P - 300, P + 200, P - 100, P + 100};
    OrderId id = 1;
    for (Price p : prices) {
        ASSERT_EQ(b.add(L1, id++, Side::Buy, 10, p), Status::Ok);
        ASSERT_EQ(b.add(L1, id++, Side::Sell, 10, p + 1'000), Status::Ok);
    }
    std::vector<Price> bids;
    std::vector<Price> asks;
    for (const auto& lv : levels(b, L1, Side::Buy)) {
        bids.push_back(lv.price);
    }
    for (const auto& lv : levels(b, L1, Side::Sell)) {
        asks.push_back(lv.price);
    }
    EXPECT_EQ(bids, (std::vector<Price>{P + 200, P + 100, P, P - 100, P - 300}));
    EXPECT_EQ(asks, (std::vector<Price>{P + 700, P + 900, P + 1'000, P + 1'100, P + 1'200}));
    EXPECT_EQ(b.best(L1, Side::Buy)->price, P + 200);
    EXPECT_EQ(b.best(L1, Side::Sell)->price, P + 700);
}

TYPED_TEST_P(BookContract, SameLevelIsFifoAndAggregated) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 3, Side::Sell, 30, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 1, Side::Sell, 10, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Sell, 20, P), Status::Ok);
    EXPECT_EQ(ids_at(b, L1, Side::Sell, P), (std::vector<OrderId>{3, 1, 2}));
    EXPECT_EQ(b.best(L1, Side::Sell), (Level{.price = P, .qty = 60, .orders = 3}));
}

TYPED_TEST_P(BookContract, CrossedBookIsStoredNotMatched) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Sell, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Buy, 100, P + 100), Status::Ok);
    EXPECT_EQ(b.best(L1, Side::Buy)->price, P + 100);
    EXPECT_EQ(b.best(L1, Side::Sell)->price, P);
    EXPECT_EQ(b.order_count(), 2U);
}

TYPED_TEST_P(BookContract, ExtremeValues) {
    auto& b = this->book;
    constexpr Locate max_l = std::numeric_limits<Locate>::max();
    constexpr OrderId max_id = std::numeric_limits<OrderId>::max();
    constexpr Price max_p = std::numeric_limits<Price>::max();
    ASSERT_EQ(b.add(0, 0, Side::Buy, 1, 1), Status::Ok);
    ASSERT_EQ(b.add(max_l, max_id, Side::Sell, std::numeric_limits<Qty>::max(), max_p), Status::Ok);
    ASSERT_EQ(b.add(max_l, 5, Side::Buy, 1, 0), Status::Ok);
    ASSERT_EQ(b.add(L1, 6, Side::Buy, 1, 1), Status::Ok);
    ASSERT_EQ(b.add(L1, 7, Side::Buy, 1, 2'000'000'000), Status::Ok);
    EXPECT_EQ(b.best(0, Side::Buy), (Level{.price = 1, .qty = 1, .orders = 1}));
    EXPECT_EQ(b.best(max_l, Side::Sell),
              (Level{.price = max_p, .qty = std::numeric_limits<Qty>::max(), .orders = 1}));
    EXPECT_EQ(b.best(max_l, Side::Buy)->price, 0U);
    EXPECT_EQ(levels(b, L1, Side::Buy).size(), 2U);
    EXPECT_EQ(b.find(max_id)->locate, max_l);
    EXPECT_EQ(b.remove(max_l, max_id), Status::Ok);
    EXPECT_EQ(b.remove(0, 0), Status::Ok);
}

TYPED_TEST_P(BookContract, LevelQtyDoesNotOverflow32Bits) {
    auto& b = this->book;
    constexpr Qty big = std::numeric_limits<Qty>::max();
    ASSERT_EQ(b.add(L1, 1, Side::Buy, big, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Buy, big, P), Status::Ok);
    EXPECT_EQ(b.best(L1, Side::Buy)->qty, 2ULL * big);
}

// ---- Executions and cancels --------------------------------------------------

TYPED_TEST_P(BookContract, PartialExecutionKeepsPriority) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Buy, 100, P), Status::Ok);
    EXPECT_EQ(b.execute(L1, 1, 40), Status::Ok);
    EXPECT_EQ(b.find(1)->qty, 60U);
    EXPECT_EQ(ids_at(b, L1, Side::Buy, P), (std::vector<OrderId>{1, 2}));
    EXPECT_EQ(b.best(L1, Side::Buy), (Level{.price = P, .qty = 160, .orders = 2}));
}

TYPED_TEST_P(BookContract, FullExecutionRemovesOrderAndEmptiesLevel) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Buy, 100, P - 100), Status::Ok);
    EXPECT_EQ(b.execute(L1, 1, 60), Status::Ok);
    EXPECT_EQ(b.execute(L1, 1, 40), Status::Ok);
    EXPECT_FALSE(b.find(1));
    EXPECT_EQ(b.order_count(), 1U);
    EXPECT_EQ(b.best(L1, Side::Buy)->price, P - 100) << "emptied level must disappear";
    EXPECT_TRUE(ids_at(b, L1, Side::Buy, P).empty());
    EXPECT_EQ(b.execute(L1, 2, 100), Status::Ok);
    EXPECT_FALSE(b.best(L1, Side::Buy));
    EXPECT_EQ(b.order_count(), 0U);
}

TYPED_TEST_P(BookContract, ExecutionWithPriceReducesAtRestingPrice) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Sell, 100, P), Status::Ok);
    const itch::OrderExecutedWithPrice c{
        .header = {.stock_locate = L1, .tracking_number = 0, .timestamp = 0},
        .order_reference_number = 1,
        .executed_shares = 30,
        .match_number = 1,
        .printable = 'Y',
        .execution_price = P - 500};
    EXPECT_EQ(book::apply(b, itch::AnyMessage{c}), Status::Ok);
    EXPECT_EQ(levels(b, L1, Side::Sell),
              (std::vector<Level>{{.price = P, .qty = 70, .orders = 1}}));
}

TYPED_TEST_P(BookContract, PartialAndFullCancel) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Sell, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Sell, 100, P), Status::Ok);
    EXPECT_EQ(b.cancel(L1, 1, 30), Status::Ok);
    EXPECT_EQ(ids_at(b, L1, Side::Sell, P), (std::vector<OrderId>{1, 2}))
        << "cancel keeps priority";
    EXPECT_EQ(b.cancel(L1, 1, 70), Status::Ok);
    EXPECT_FALSE(b.find(1));
    EXPECT_EQ(b.best(L1, Side::Sell), (Level{.price = P, .qty = 100, .orders = 1}));
}

TYPED_TEST_P(BookContract, OverfillRemovesOrder) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Buy, 100, P), Status::Ok);
    EXPECT_EQ(b.execute(L1, 1, 101), Status::Overfill);
    EXPECT_FALSE(b.find(1));
    EXPECT_EQ(b.cancel(L1, 2, 500), Status::Overfill);
    EXPECT_FALSE(b.find(2));
    EXPECT_FALSE(b.best(L1, Side::Buy));
    EXPECT_EQ(b.order_count(), 0U);
}

// ---- Deletes and replaces ----------------------------------------------------

TYPED_TEST_P(BookContract, DeleteFromMiddleOfQueue) {
    auto& b = this->book;
    for (OrderId id = 1; id <= 3; ++id) {
        ASSERT_EQ(b.add(L1, id, Side::Buy, 100, P), Status::Ok);
    }
    EXPECT_EQ(b.remove(L1, 2), Status::Ok);
    EXPECT_EQ(ids_at(b, L1, Side::Buy, P), (std::vector<OrderId>{1, 3}));
    EXPECT_EQ(b.remove(L1, 1), Status::Ok);
    EXPECT_EQ(b.remove(L1, 3), Status::Ok);
    EXPECT_FALSE(b.best(L1, Side::Buy));
}

TYPED_TEST_P(BookContract, ReplaceSamePriceLosesPriority) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Sell, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Sell, 100, P), Status::Ok);
    EXPECT_EQ(b.replace(L1, 1, 3, 100, P), Status::Ok);
    EXPECT_EQ(ids_at(b, L1, Side::Sell, P), (std::vector<OrderId>{2, 3}));
    EXPECT_FALSE(b.find(1));
    EXPECT_EQ(b.find(3),
              (Order{.id = 3, .locate = L1, .side = Side::Sell, .price = P, .qty = 100}));
}

TYPED_TEST_P(BookContract, ReplaceNewPriceAndSizeKeepsSide) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    EXPECT_EQ(b.replace(L1, 1, 2, 500, P - 200), Status::Ok);
    EXPECT_EQ(levels(b, L1, Side::Buy),
              (std::vector<Level>{{.price = P - 200, .qty = 500, .orders = 1}}));
    EXPECT_EQ(b.find(2)->side, Side::Buy);
    EXPECT_EQ(b.order_count(), 1U);
}

TYPED_TEST_P(BookContract, IdReusableAfterRemoval) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    ASSERT_EQ(b.remove(L1, 1), Status::Ok);
    EXPECT_EQ(b.add(L1, 1, Side::Sell, 50, P + 100), Status::Ok);
    EXPECT_EQ(b.find(1)->side, Side::Sell);
}

// ---- Contract violations: no state change ------------------------------------

TYPED_TEST_P(BookContract, UnknownIdsAreNoOps) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    this->expect_noop(L1, Status::UnknownOrder, [&] { return b.execute(L1, 99, 10); });
    this->expect_noop(L1, Status::UnknownOrder, [&] { return b.cancel(L1, 99, 10); });
    this->expect_noop(L1, Status::UnknownOrder, [&] { return b.remove(L1, 99); });
    this->expect_noop(L1, Status::UnknownOrder, [&] { return b.replace(L1, 99, 100, 10, P); });
}

TYPED_TEST_P(BookContract, ZeroQuantitiesAreNoOps) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    this->expect_noop(L1, Status::ZeroQuantity, [&] { return b.add(L1, 2, Side::Buy, 0, P); });
    this->expect_noop(L1, Status::ZeroQuantity, [&] { return b.execute(L1, 1, 0); });
    this->expect_noop(L1, Status::ZeroQuantity, [&] { return b.cancel(L1, 1, 0); });
    this->expect_noop(L1, Status::ZeroQuantity, [&] { return b.replace(L1, 1, 2, 0, P); });
    EXPECT_FALSE(b.find(2));
}

TYPED_TEST_P(BookContract, DuplicateIdsAreNoOps) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    ASSERT_EQ(b.add(L1, 2, Side::Buy, 100, P - 100), Status::Ok);
    this->expect_noop(L1, Status::DuplicateOrder,
                      [&] { return b.add(L1, 1, Side::Sell, 5, P + 9); });
    this->expect_noop(L2, Status::DuplicateOrder, [&] { return b.add(L2, 1, Side::Sell, 5, P); });
    this->expect_noop(L1, Status::DuplicateOrder, [&] { return b.replace(L1, 1, 2, 10, P); });
    this->expect_noop(L1, Status::DuplicateOrder, [&] { return b.replace(L1, 1, 1, 10, P); });
}

TYPED_TEST_P(BookContract, LocateMismatchIsNoOp) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    this->expect_noop(L1, Status::LocateMismatch, [&] { return b.execute(L2, 1, 10); });
    this->expect_noop(L1, Status::LocateMismatch, [&] { return b.cancel(L2, 1, 10); });
    this->expect_noop(L1, Status::LocateMismatch, [&] { return b.remove(L2, 1); });
    this->expect_noop(L1, Status::LocateMismatch, [&] { return b.replace(L2, 1, 2, 10, P); });
}

TYPED_TEST_P(BookContract, CheckPrecedence) {
    auto& b = this->book;
    ASSERT_EQ(b.add(L1, 1, Side::Buy, 100, P), Status::Ok);
    EXPECT_EQ(b.add(L1, 1, Side::Buy, 0, P), Status::ZeroQuantity) << "zero before duplicate";
    EXPECT_EQ(b.execute(L2, 1, 0), Status::LocateMismatch) << "locate before zero";
    EXPECT_EQ(b.replace(L2, 1, 1, 0, P), Status::LocateMismatch);
    EXPECT_EQ(b.replace(L1, 1, 1, 0, P), Status::ZeroQuantity) << "zero before duplicate";
    EXPECT_EQ(b.execute(L2, 99, 0), Status::UnknownOrder) << "unknown first";
}

// ---- Feeds ---------------------------------------------------------------------

TYPED_TEST_P(BookContract, EdgeFeedFinalState) {
    auto& b = this->book;
    for (const auto& m : sim::generate_edge_cases()) {
        const auto st = book::apply(b, m);
        ASSERT_TRUE(!st || *st == Status::Ok) << "valid feed, type " << itch::type_of(m);
    }
    // Locate 1 after the scripted sequence (src/sim/generator.cpp).
    EXPECT_EQ(levels(b, L1, Side::Buy),
              (std::vector<Level>{{.price = 99'000, .qty = 200, .orders = 2}}));
    EXPECT_EQ(ids_at(b, L1, Side::Buy, 99'000), (std::vector<OrderId>{12, 14}));
    EXPECT_EQ(levels(b, L1, Side::Sell),
              (std::vector<Level>{{.price = P + 100, .qty = 100, .orders = 1},
                                  {.price = P + 300, .qty = 500, .orders = 1}}));
    EXPECT_EQ(ids_at(b, L1, Side::Sell, P + 100), (std::vector<OrderId>{7}));
    EXPECT_EQ(ids_at(b, L1, Side::Sell, P + 300), (std::vector<OrderId>{8}));
    EXPECT_FALSE(b.best(L2, Side::Buy));
    EXPECT_EQ(b.best(L2, Side::Sell), (Level{.price = 2'000'000'000, .qty = 100, .orders = 1}));
    EXPECT_FALSE(b.best(65'535, Side::Buy));
    EXPECT_EQ(b.order_count(), 5U);
}

TYPED_TEST_P(BookContract, ViolationsFeedStatuses) {
    auto& b = this->book;
    std::vector<Status> got;
    for (const auto& m : sim::generate_contract_violations()) {
        if (const auto st = book::apply(b, m)) {
            got.push_back(*st);
        }
    }
    using S = Status;
    const std::vector<Status> expected{S::ZeroQuantity,
                                       S::Ok,
                                       S::DuplicateOrder,
                                       S::UnknownOrder,
                                       S::UnknownOrder,
                                       S::UnknownOrder,
                                       S::UnknownOrder,
                                       S::UnknownOrder,
                                       S::ZeroQuantity,
                                       S::ZeroQuantity,
                                       S::Overfill,
                                       S::Ok,
                                       S::Ok,
                                       S::Ok,
                                       S::DuplicateOrder,
                                       S::DuplicateOrder,
                                       S::ZeroQuantity,
                                       S::LocateMismatch,
                                       S::LocateMismatch,
                                       S::LocateMismatch,
                                       S::LocateMismatch,
                                       S::Overfill,
                                       S::UnknownOrder,
                                       S::Ok,
                                       S::Ok};
    EXPECT_EQ(got, expected);
    EXPECT_EQ(b.order_count(), 2U); // 101 and 103
}

TYPED_TEST_P(BookContract, RandomFeedIsAllOk) {
    auto& b = this->book;
    sim::GeneratorConfig cfg;
    cfg.book_messages = 5'000;
    for (const auto& m : sim::generate_random(cfg)) {
        const auto st = book::apply(b, m);
        ASSERT_TRUE(!st || *st == Status::Ok) << "type " << itch::type_of(m);
    }
}

REGISTER_TYPED_TEST_SUITE_P(BookContract, AddCreatesLevelAndOrder, LevelsSortedBestFirst,
                            SameLevelIsFifoAndAggregated, CrossedBookIsStoredNotMatched,
                            ExtremeValues, LevelQtyDoesNotOverflow32Bits,
                            PartialExecutionKeepsPriority, FullExecutionRemovesOrderAndEmptiesLevel,
                            ExecutionWithPriceReducesAtRestingPrice, PartialAndFullCancel,
                            OverfillRemovesOrder, DeleteFromMiddleOfQueue,
                            ReplaceSamePriceLosesPriority, ReplaceNewPriceAndSizeKeepsSide,
                            IdReusableAfterRemoval, UnknownIdsAreNoOps, ZeroQuantitiesAreNoOps,
                            DuplicateIdsAreNoOps, LocateMismatchIsNoOp, CheckPrecedence,
                            EdgeFeedFinalState, ViolationsFeedStatuses, RandomFeedIsAllOk);

} // namespace tickpath::test
