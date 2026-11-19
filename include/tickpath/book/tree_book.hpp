#pragma once

// Tree layout: price levels in a balanced tree (or similar ordered structure)
// per (symbol, side).
//
// OWNER-WRITTEN HOT PATH. Every body below is a stub returning NotImplemented
// (or empty). Interface: OrderBook concept (concept.hpp). Semantics:
// docs/BOOK_CONTRACT.md. Tests: tests/book/ (contract, differential, replay)
// and the hot-path guard.
//
// Requirements on the implementation:
//   - all operations noexcept;
//   - no allocation after construction: size every pool from BookConfig;
//     return CapacityExceeded when a pool is full;
//   - identical observable state to ReferenceBook after every message.

#include <cstddef>
#include <optional>

#include "tickpath/book/concept.hpp"
#include "tickpath/book/types.hpp"

namespace tickpath::book {

class TreeBook {
public:
    explicit TreeBook(const BookConfig& cfg) { static_cast<void>(cfg); }

    // OWNER: implement every member below. Delete the NOLINT markers with the stubs.
    // NOLINTBEGIN(readability-convert-member-functions-to-static)

    Status add(Locate l, OrderId id, Side s, Qty q, Price p) noexcept {
        static_cast<void>(l), static_cast<void>(id), static_cast<void>(s), static_cast<void>(q),
            static_cast<void>(p);
        return Status::NotImplemented;
    }

    Status execute(Locate l, OrderId id, Qty q) noexcept {
        static_cast<void>(l), static_cast<void>(id), static_cast<void>(q);
        return Status::NotImplemented;
    }

    Status cancel(Locate l, OrderId id, Qty q) noexcept {
        static_cast<void>(l), static_cast<void>(id), static_cast<void>(q);
        return Status::NotImplemented;
    }

    Status remove(Locate l, OrderId id) noexcept {
        static_cast<void>(l), static_cast<void>(id);
        return Status::NotImplemented;
    }

    Status replace(Locate l, OrderId old_id, OrderId new_id, Qty q, Price p) noexcept {
        static_cast<void>(l), static_cast<void>(old_id), static_cast<void>(new_id),
            static_cast<void>(q), static_cast<void>(p);
        return Status::NotImplemented;
    }

    [[nodiscard]] std::optional<Level> best(Locate l, Side s) const noexcept {
        static_cast<void>(l), static_cast<void>(s);
        return std::nullopt;
    }

    [[nodiscard]] std::optional<Order> find(OrderId id) const noexcept {
        static_cast<void>(id);
        return std::nullopt;
    }

    [[nodiscard]] std::size_t order_count() const noexcept { return 0; }

    /// Levels of one side, best first.
    template <class F>
    void for_each_level(Locate l, Side s, F fn) const noexcept {
        static_cast<void>(l), static_cast<void>(s), static_cast<void>(fn);
    }

    /// Orders at one level, oldest first.
    template <class F>
    void for_each_order(Locate l, Side s, Price p, F fn) const noexcept {
        static_cast<void>(l), static_cast<void>(s), static_cast<void>(p), static_cast<void>(fn);
    }
    // NOLINTEND(readability-convert-member-functions-to-static)
};

static_assert(OrderBook<TreeBook>);

} // namespace tickpath::book
