#pragma once

// Canonical hash of book state, defined only through the OrderBook concept, so
// every implementation that holds the same state yields the same value.
// Covers, per locate and side: levels best-first (price, qty, count) and the
// orders of each level in FIFO order (id, qty).

#include <cstdint>
#include <limits>

#include "tickpath/book/concept.hpp"
#include "tickpath/util/fnv1a.hpp"

namespace tickpath::book {

template <OrderBook B>
void hash_symbol(const B& b, Locate l, util::Fnv1a& h) noexcept {
    for (const Side s : {Side::Buy, Side::Sell}) {
        b.for_each_level(l, s, [&](const Level& lv) noexcept {
            h.integer(l);
            h.integer(static_cast<std::uint8_t>(s));
            h.integer(lv.price);
            h.integer(lv.qty);
            h.integer(lv.orders);
            b.for_each_order(l, s, lv.price, [&](const Order& o) noexcept {
                h.integer(o.id);
                h.integer(o.qty);
            });
        });
    }
}

/// Hash of one symbol.
template <OrderBook B>
[[nodiscard]] std::uint64_t symbol_hash(const B& b, Locate l) noexcept {
    util::Fnv1a h;
    hash_symbol(b, l, h);
    return h.value();
}

/// Hash of the whole market: every locate 0..65535, plus the live order count.
template <OrderBook B>
[[nodiscard]] std::uint64_t state_hash(const B& b) noexcept {
    util::Fnv1a h;
    for (std::uint32_t l = 0; l <= std::numeric_limits<Locate>::max(); ++l) {
        hash_symbol(b, static_cast<Locate>(l), h);
    }
    h.integer(static_cast<std::uint64_t>(b.order_count()));
    return h.value();
}

} // namespace tickpath::book
