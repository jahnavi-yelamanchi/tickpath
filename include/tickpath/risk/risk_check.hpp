#pragma once

// Pre-trade risk check.
//
// OWNER-WRITTEN HOT PATH. Member bodies are stubs. Semantics:
// docs/RISK_CONTRACT.md. Tests: tests/risk/.
//
// Requirements on the implementation:
//   - integer arithmetic only, no floating point, no overflow for any input
//     within the types below;
//   - check() is noexcept, does not allocate and does not modify state.

#include <cstdint>
#include <optional>

#include "tickpath/book/types.hpp"

namespace tickpath::risk {

using book::Locate;
using book::Price;
using book::Qty;
using book::Side;

inline constexpr std::uint32_t max_collar_bps = 1'000'000; ///< 10,000 %

struct Limits {
    Qty max_order_qty{}; ///< largest single order, shares
    std::uint32_t
        collar_bps{}; ///< max distance from the reference price, basis points, <= max_collar_bps
    std::int64_t max_abs_position{}; ///< per symbol, shares, >= 0
};

/// An order we intend to send.
struct OrderRequest {
    Locate locate{};
    Side side{Side::Buy};
    Qty qty{};
    Price price{};
};

/// Best bid and offer of the symbol at decision time (from the book).
struct Bbo {
    std::optional<Price> bid;
    std::optional<Price> ask;
};

enum class Verdict : std::uint8_t {
    Accept,
    KillSwitch,     ///< kill switch engaged
    ZeroQuantity,   ///< qty == 0
    MaxOrderSize,   ///< qty > max_order_qty
    NoReference,    ///< the side used as collar reference is empty
    PriceCollar,    ///< price outside the collar
    PositionLimit,  ///< |position after full fill| > max_abs_position
    NotImplemented, ///< stub
};

class RiskCheck {
public:
    /// Preconditions: limits.collar_bps <= max_collar_bps, limits.max_abs_position >= 0.
    explicit RiskCheck(const Limits& limits) noexcept
        : limits_(limits) {}

    // OWNER: implement every member below and add the state they need. Delete the NOLINT
    // markers with the stubs.
    // NOLINTBEGIN(readability-convert-member-functions-to-static)

    /// Pure decision; checks run in the order of Verdict's enumerators.
    [[nodiscard]] Verdict check(const OrderRequest& order, const Bbo& bbo) const noexcept {
        static_cast<void>(order), static_cast<void>(bbo);
        return Verdict::NotImplemented;
    }

    /// Applies a fill to the symbol's position: buy adds, sell subtracts.
    void on_fill(Locate locate, Side side, Qty qty) noexcept {
        static_cast<void>(locate), static_cast<void>(side), static_cast<void>(qty);
    }

    [[nodiscard]] std::int64_t position(Locate locate) const noexcept {
        static_cast<void>(locate);
        return 0;
    }

    void engage_kill_switch() noexcept {}
    void release_kill_switch() noexcept {}
    [[nodiscard]] bool kill_switch_engaged() const noexcept { return false; }
    // NOLINTEND(readability-convert-member-functions-to-static)

    [[nodiscard]] const Limits& limits() const noexcept { return limits_; }

private:
    Limits limits_;
};

[[nodiscard]] constexpr const char* to_string(Verdict v) noexcept {
    switch (v) {
    case Verdict::Accept:
        return "Accept";
    case Verdict::KillSwitch:
        return "KillSwitch";
    case Verdict::ZeroQuantity:
        return "ZeroQuantity";
    case Verdict::MaxOrderSize:
        return "MaxOrderSize";
    case Verdict::NoReference:
        return "NoReference";
    case Verdict::PriceCollar:
        return "PriceCollar";
    case Verdict::PositionLimit:
        return "PositionLimit";
    case Verdict::NotImplemented:
        return "NotImplemented";
    }
    return "?";
}

} // namespace tickpath::risk
