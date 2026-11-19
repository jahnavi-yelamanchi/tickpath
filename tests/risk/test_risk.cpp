// OWNER: risk check tests (docs/RISK_CONTRACT.md).

#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "tickpath/risk/risk_check.hpp"

namespace tickpath::risk {
inline std::ostream& operator<<(std::ostream& os, Verdict v) {
    return os << to_string(v);
}
} // namespace tickpath::risk

namespace {

using namespace tickpath::risk;

constexpr Price bid = 990'000;   // $99.0000
constexpr Price ask = 1'000'000; // $100.0000
constexpr Limits limits{.max_order_qty = 1'000, .collar_bps = 500, .max_abs_position = 5'000};
constexpr Bbo bbo{.bid = bid, .ask = ask};
constexpr Locate L = 7;

OrderRequest buy(Qty q, Price p, Locate l = L) {
    return {.locate = l, .side = Side::Buy, .qty = q, .price = p};
}
OrderRequest sell(Qty q, Price p, Locate l = L) {
    return {.locate = l, .side = Side::Sell, .qty = q, .price = p};
}

} // namespace

TEST(Risk, AcceptsOrdinaryOrders) {
    const RiskCheck r(limits);
    EXPECT_EQ(r.check(buy(100, ask), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(sell(100, bid), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(buy(100, bid), bbo), Verdict::Accept) << "passive buy";
    EXPECT_EQ(r.check(buy(100, 1), bbo), Verdict::Accept) << "collar bounds crossing only";
}

TEST(Risk, KillSwitch) {
    RiskCheck r(limits);
    EXPECT_FALSE(r.kill_switch_engaged());
    r.engage_kill_switch();
    EXPECT_TRUE(r.kill_switch_engaged());
    EXPECT_EQ(r.check(buy(100, ask), bbo), Verdict::KillSwitch);
    EXPECT_EQ(r.check(buy(0, ask), bbo), Verdict::KillSwitch) << "first check";
    r.release_kill_switch();
    EXPECT_FALSE(r.kill_switch_engaged());
    EXPECT_EQ(r.check(buy(100, ask), bbo), Verdict::Accept);
}

TEST(Risk, ZeroQuantity) {
    const RiskCheck r(limits);
    EXPECT_EQ(r.check(buy(0, ask), bbo), Verdict::ZeroQuantity);
    EXPECT_EQ(r.check(sell(0, bid), Bbo{}), Verdict::ZeroQuantity) << "before reference check";
}

TEST(Risk, MaxOrderSizeBoundary) {
    const RiskCheck r(limits);
    EXPECT_EQ(r.check(buy(1'000, ask), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(buy(1'001, ask), bbo), Verdict::MaxOrderSize);
    EXPECT_EQ(r.check(sell(std::numeric_limits<Qty>::max(), bid), Bbo{}), Verdict::MaxOrderSize)
        << "before reference check";
}

TEST(Risk, NoReferenceUsesFarSide) {
    const RiskCheck r(limits);
    EXPECT_EQ(r.check(buy(100, ask), Bbo{.bid = bid, .ask = std::nullopt}), Verdict::NoReference);
    EXPECT_EQ(r.check(sell(100, bid), Bbo{.bid = std::nullopt, .ask = ask}), Verdict::NoReference);
    EXPECT_EQ(r.check(buy(100, ask), Bbo{.bid = std::nullopt, .ask = ask}), Verdict::Accept);
    EXPECT_EQ(r.check(sell(100, bid), Bbo{.bid = bid, .ask = std::nullopt}), Verdict::Accept);
}

TEST(Risk, BuyCollarBoundary) {
    const RiskCheck r(limits);
    // 5% above the ask: 1,050,000 is on the boundary.
    EXPECT_EQ(r.check(buy(100, 1'050'000), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(buy(100, 1'050'001), bbo), Verdict::PriceCollar);
}

TEST(Risk, SellCollarBoundary) {
    const RiskCheck r(limits);
    // 5% below the bid: 990,000 * 0.95 = 940,500.
    EXPECT_EQ(r.check(sell(100, 940'500), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(sell(100, 940'499), bbo), Verdict::PriceCollar);
}

TEST(Risk, CollarRoundingIsExact) {
    // ask * 10003 / 10000 is not an integer; no rounding allowed either way.
    const RiskCheck r(Limits{.max_order_qty = 10, .collar_bps = 3, .max_abs_position = 100});
    const Bbo b{.bid = 333, .ask = 333};
    // 333 * 1.0003 = 333.0999 -> 333 ok, 334 rejected.
    EXPECT_EQ(r.check(buy(1, 333), b), Verdict::Accept);
    EXPECT_EQ(r.check(buy(1, 334), b), Verdict::PriceCollar);
    // 333 * 0.9997 = 332.9001 -> 333 ok, 332 rejected.
    EXPECT_EQ(r.check(sell(1, 333), b), Verdict::Accept);
    EXPECT_EQ(r.check(sell(1, 332), b), Verdict::PriceCollar);
}

TEST(Risk, ZeroCollar) {
    const RiskCheck r(Limits{.max_order_qty = 10, .collar_bps = 0, .max_abs_position = 100});
    EXPECT_EQ(r.check(buy(1, ask), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(buy(1, ask + 1), bbo), Verdict::PriceCollar);
    EXPECT_EQ(r.check(sell(1, bid), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(sell(1, bid - 1), bbo), Verdict::PriceCollar);
}

TEST(Risk, WideCollarHasNoNegativeLowerBound) {
    const RiskCheck r(Limits{.max_order_qty = 10, .collar_bps = 20'000, .max_abs_position = 100});
    EXPECT_EQ(r.check(sell(1, 0), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(buy(1, 3 * ask), bbo), Verdict::Accept);
    EXPECT_EQ(r.check(buy(1, 3 * ask + 1), bbo), Verdict::PriceCollar);
}

TEST(Risk, CollarArithmeticDoesNotOverflow) {
    constexpr Price max_p = std::numeric_limits<Price>::max();
    const RiskCheck wide(
        Limits{.max_order_qty = 10, .collar_bps = max_collar_bps, .max_abs_position = 100});
    EXPECT_EQ(wide.check(buy(1, max_p), Bbo{.bid = max_p, .ask = max_p}), Verdict::Accept);
    EXPECT_EQ(wide.check(sell(1, 0), Bbo{.bid = max_p, .ask = max_p}), Verdict::Accept);
    const RiskCheck tight(Limits{.max_order_qty = 10, .collar_bps = 0, .max_abs_position = 100});
    EXPECT_EQ(tight.check(buy(1, max_p), Bbo{.bid = 1, .ask = 1}), Verdict::PriceCollar);
    EXPECT_EQ(tight.check(sell(1, 0), Bbo{.bid = max_p, .ask = max_p}), Verdict::PriceCollar);
    EXPECT_EQ(tight.check(buy(1, max_p), Bbo{.bid = max_p, .ask = max_p}), Verdict::Accept);
}

TEST(Risk, PositionFromFills) {
    RiskCheck r(limits);
    EXPECT_EQ(r.position(L), 0);
    r.on_fill(L, Side::Buy, 300);
    r.on_fill(L, Side::Sell, 100);
    EXPECT_EQ(r.position(L), 200);
    EXPECT_EQ(r.position(L + 1), 0) << "positions are per symbol";
    r.on_fill(L + 1, Side::Sell, 50);
    EXPECT_EQ(r.position(L + 1), -50);
}

TEST(Risk, PositionLimitBoundaryLong) {
    RiskCheck r(limits);
    r.on_fill(L, Side::Buy, 4'000);
    EXPECT_EQ(r.check(buy(1'000, ask), bbo), Verdict::Accept) << "exactly at limit";
    r.on_fill(L, Side::Buy, 1);
    EXPECT_EQ(r.check(buy(1'000, ask), bbo), Verdict::PositionLimit);
    EXPECT_EQ(r.check(sell(1'000, bid), bbo), Verdict::Accept) << "reducing is allowed";
}

TEST(Risk, PositionLimitBoundaryShort) {
    RiskCheck r(limits);
    r.on_fill(L, Side::Sell, 4'000);
    EXPECT_EQ(r.check(sell(1'000, bid), bbo), Verdict::Accept);
    r.on_fill(L, Side::Sell, 1);
    EXPECT_EQ(r.check(sell(1'000, bid), bbo), Verdict::PositionLimit);
    EXPECT_EQ(r.check(buy(1'000, ask), bbo), Verdict::Accept);
}

TEST(Risk, ZeroPositionLimitRejectsEverything) {
    const RiskCheck r(Limits{.max_order_qty = 10, .collar_bps = 500, .max_abs_position = 0});
    EXPECT_EQ(r.check(buy(1, ask), bbo), Verdict::PositionLimit);
    EXPECT_EQ(r.check(sell(1, bid), bbo), Verdict::PositionLimit);
}

TEST(Risk, CheckPrecedence) {
    RiskCheck r(limits);
    r.on_fill(L, Side::Buy, 5'000);
    EXPECT_EQ(r.check(buy(2'000, 9'999'999), Bbo{}), Verdict::MaxOrderSize);
    EXPECT_EQ(r.check(buy(100, 9'999'999), Bbo{}), Verdict::NoReference);
    EXPECT_EQ(r.check(buy(100, 9'999'999), bbo), Verdict::PriceCollar);
    EXPECT_EQ(r.check(buy(100, ask), bbo), Verdict::PositionLimit);
}

TEST(Risk, CheckDoesNotChangeState) {
    RiskCheck r(limits);
    r.on_fill(L, Side::Buy, 10);
    for (int i = 0; i < 3; ++i) {
        static_cast<void>(r.check(buy(100, ask), bbo));
    }
    EXPECT_EQ(r.position(L), 10);
}

// ---- Properties: verdict equals the contract's definition ----------------------

namespace {

__extension__ using i128 = __int128; // exact reference arithmetic, wider than the code under test

Verdict model(const Limits& lim, bool killed, std::int64_t pos, const OrderRequest& o,
              const Bbo& b) {
    if (killed) {
        return Verdict::KillSwitch;
    }
    if (o.qty == 0) {
        return Verdict::ZeroQuantity;
    }
    if (o.qty > lim.max_order_qty) {
        return Verdict::MaxOrderSize;
    }
    const auto ref = o.side == Side::Buy ? b.ask : b.bid;
    if (!ref) {
        return Verdict::NoReference;
    }
    const i128 px = i128{o.price} * 10'000;
    const i128 bps = lim.collar_bps;
    if (o.side == Side::Buy ? px > i128{*ref} * (10'000 + bps) : px < i128{*ref} * (10'000 - bps)) {
        return Verdict::PriceCollar;
    }
    const i128 next = i128{pos} + (o.side == Side::Buy ? i128{o.qty} : -i128{o.qty});
    if ((next < 0 ? -next : next) > lim.max_abs_position) {
        return Verdict::PositionLimit;
    }
    return Verdict::Accept;
}

} // namespace

RC_GTEST_PROP(RiskProp, MatchesContract, ()) {
    const Limits lim{.max_order_qty = *rc::gen::inRange<Qty>(0, 100'000),
                     .collar_bps = *rc::gen::inRange<std::uint32_t>(0, max_collar_bps + 1),
                     .max_abs_position = *rc::gen::inRange<std::int64_t>(0, 1'000'000)};
    RiskCheck r(lim);
    const bool killed = *rc::gen::arbitrary<bool>();
    if (killed) {
        r.engage_kill_switch();
    }
    std::int64_t pos = 0;
    const auto fills = *rc::gen::container<std::vector<std::pair<bool, Qty>>>(
        rc::gen::pair(rc::gen::arbitrary<bool>(), rc::gen::inRange<Qty>(0, 100'000)));
    for (const auto& [is_buy, q] : fills) {
        r.on_fill(L, is_buy ? Side::Buy : Side::Sell, q);
        pos += is_buy ? std::int64_t{q} : -std::int64_t{q};
    }
    RC_ASSERT(r.position(L) == pos);

    const auto maybe_price = []() -> std::optional<Price> {
        if (*rc::gen::arbitrary<bool>()) {
            return std::nullopt;
        }
        return *rc::gen::arbitrary<Price>();
    };
    const Bbo b{.bid = maybe_price(), .ask = maybe_price()};
    const OrderRequest o{
        .locate = L,
        .side = *rc::gen::element(Side::Buy, Side::Sell),
        .qty = *rc::gen::oneOf(rc::gen::inRange<Qty>(0, 200'000), rc::gen::arbitrary<Qty>()),
        .price = *rc::gen::arbitrary<Price>()};
    RC_ASSERT(r.check(o, b) == model(lim, killed, pos, o, b));
}
