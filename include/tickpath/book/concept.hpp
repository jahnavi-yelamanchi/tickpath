#pragma once

// The compile-time interface every book implements. Implementations are
// selected by template parameter; there are no virtual calls (ADR-010).

#include <concepts>
#include <cstddef>
#include <optional>

#include "tickpath/book/types.hpp"

namespace tickpath::book {

namespace detail {
struct LevelProbe {
    void operator()(const Level& /*level*/) const noexcept {}
};
struct OrderProbe {
    void operator()(const Order& /*order*/) const noexcept {}
};
} // namespace detail

/// Market-wide limit order book: every symbol (locate), both sides, every
/// order. All operations are noexcept; mutations return a Status.
template <class B>
concept OrderBook = std::constructible_from<B, const BookConfig&> &&
                    requires(B& b, const B& cb, Locate l, OrderId id, Side s, Qty q, Price p,
                             detail::LevelProbe level_fn, detail::OrderProbe order_fn) {
                        // Mutations
                        { b.add(l, id, s, q, p) } noexcept -> std::same_as<Status>;
                        { b.execute(l, id, q) } noexcept -> std::same_as<Status>;
                        { b.cancel(l, id, q) } noexcept -> std::same_as<Status>;
                        { b.remove(l, id) } noexcept -> std::same_as<Status>;
                        { b.replace(l, id, id, q, p) } noexcept -> std::same_as<Status>;
                        // Queries
                        { cb.best(l, s) } noexcept -> std::same_as<std::optional<Level>>;
                        { cb.find(id) } noexcept -> std::same_as<std::optional<Order>>;
                        { cb.order_count() } noexcept -> std::same_as<std::size_t>;
                        /// Levels of one side, best first (bids descending, asks ascending).
                        { cb.for_each_level(l, s, level_fn) } noexcept;
                        /// Orders at one level in time priority (FIFO).
                        { cb.for_each_order(l, s, p, order_fn) } noexcept;
                    };

template <OrderBook B>
[[nodiscard]] std::optional<Level> best_bid(const B& b, Locate l) noexcept {
    return b.best(l, Side::Buy);
}

template <OrderBook B>
[[nodiscard]] std::optional<Level> best_ask(const B& b, Locate l) noexcept {
    return b.best(l, Side::Sell);
}

} // namespace tickpath::book
