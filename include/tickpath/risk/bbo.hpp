#pragma once

// Reads the best bid and offer of a symbol from any book.

#include "tickpath/book/concept.hpp"
#include "tickpath/risk/risk_check.hpp"

namespace tickpath::risk {

template <book::OrderBook B>
[[nodiscard]] Bbo bbo_of(const B& b, Locate l) noexcept {
    Bbo out;
    if (const auto bid = book::best_bid(b, l)) {
        out.bid = bid->price;
    }
    if (const auto ask = book::best_ask(b, l)) {
        out.ask = ask->price;
    }
    return out;
}

} // namespace tickpath::risk
