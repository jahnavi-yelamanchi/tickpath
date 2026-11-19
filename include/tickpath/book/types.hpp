#pragma once

// Vocabulary shared by every order book implementation. Semantics of each
// operation are specified in docs/BOOK_CONTRACT.md.

#include <cstddef>
#include <cstdint>

#include "tickpath/itch/types.hpp"

namespace tickpath::book {

using itch::Locate;
using itch::Side;
using OrderId = itch::OrderRef;
using Qty = itch::Shares;
using Price = itch::Price4; ///< integer, 4 implied decimals, as on the wire

enum class Status : std::uint8_t {
    Ok,
    UnknownOrder,     ///< id not live; no state change
    DuplicateOrder,   ///< new id already live; no state change
    ZeroQuantity,     ///< quantity 0 where > 0 is required; no state change
    Overfill,         ///< execute/cancel above remaining; order removed
    LocateMismatch,   ///< message locate differs from the order's; no state change
    CapacityExceeded, ///< preallocated capacity exhausted; no state change
    NotImplemented,   ///< stub
};

/// Aggregate of one price level.
struct Level {
    Price price{};
    std::uint64_t qty{};    ///< sum of resting shares; 64-bit so it cannot overflow
    std::uint32_t orders{}; ///< number of resting orders
    friend bool operator==(const Level&, const Level&) = default;
};

/// One resting order.
struct Order {
    OrderId id{};
    Locate locate{};
    Side side{Side::Buy};
    Price price{};
    Qty qty{};
    friend bool operator==(const Order&, const Order&) = default;
};

/// Capacity hints. Fast books preallocate from these and must not allocate
/// after construction (hot-path guard). The reference book ignores them.
struct BookConfig {
    std::size_t max_orders = std::size_t{1} << 22U; ///< live orders, all symbols
    std::size_t max_levels = std::size_t{1} << 20U; ///< live price levels, all symbols and sides
};

[[nodiscard]] constexpr const char* to_string(Status s) noexcept {
    switch (s) {
    case Status::Ok:
        return "Ok";
    case Status::UnknownOrder:
        return "UnknownOrder";
    case Status::DuplicateOrder:
        return "DuplicateOrder";
    case Status::ZeroQuantity:
        return "ZeroQuantity";
    case Status::Overfill:
        return "Overfill";
    case Status::LocateMismatch:
        return "LocateMismatch";
    case Status::CapacityExceeded:
        return "CapacityExceeded";
    case Status::NotImplemented:
        return "NotImplemented";
    }
    return "?";
}

} // namespace tickpath::book
