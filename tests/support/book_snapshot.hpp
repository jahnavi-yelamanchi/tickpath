#pragma once

// Test helpers: read book state through the OrderBook concept into values
// that gtest can compare and print.

#include <ostream>
#include <vector>

#include "tickpath/book/concept.hpp"

namespace tickpath::book {

inline std::ostream& operator<<(std::ostream& os, const Level& l) {
    return os << "{px=" << l.price << " qty=" << l.qty << " n=" << l.orders << "}";
}
inline std::ostream& operator<<(std::ostream& os, const Order& o) {
    return os << "{id=" << o.id << " loc=" << o.locate << " " << static_cast<char>(o.side)
              << " px=" << o.price << " qty=" << o.qty << "}";
}
inline std::ostream& operator<<(std::ostream& os, Status s) {
    return os << to_string(s);
}

} // namespace tickpath::book

namespace tickpath::test {

using book::Level;
using book::Locate;
using book::Order;
using book::OrderBook;
using book::Price;
using book::Side;

template <OrderBook B>
std::vector<Level> levels(const B& b, Locate l, Side s) {
    std::vector<Level> out;
    b.for_each_level(l, s, [&out](const Level& lv) { out.push_back(lv); });
    return out;
}

template <OrderBook B>
std::vector<Order> orders_at(const B& b, Locate l, Side s, Price p) {
    std::vector<Order> out;
    b.for_each_order(l, s, p, [&out](const Order& o) { out.push_back(o); });
    return out;
}

template <OrderBook B>
std::vector<book::OrderId> ids_at(const B& b, Locate l, Side s, Price p) {
    std::vector<book::OrderId> out;
    b.for_each_order(l, s, p, [&out](const Order& o) { out.push_back(o.id); });
    return out;
}

/// Full state of one symbol: levels and FIFO queues of both sides.
struct SymbolSnapshot {
    std::vector<Level> bids;
    std::vector<Level> asks;
    std::vector<std::vector<Order>> bid_queues;
    std::vector<std::vector<Order>> ask_queues;
    friend bool operator==(const SymbolSnapshot&, const SymbolSnapshot&) = default;
};

inline std::ostream& operator<<(std::ostream& os, const SymbolSnapshot& s) {
    const auto side = [&os](const char* name, const auto& lvls, const auto& queues) {
        os << name << ":";
        for (std::size_t i = 0; i < lvls.size(); ++i) {
            os << " " << lvls[i] << "[";
            for (const auto& o : queues[i]) {
                os << o.id << ":" << o.qty << " ";
            }
            os << "]";
        }
        os << "\n";
    };
    side("bids", s.bids, s.bid_queues);
    side("asks", s.asks, s.ask_queues);
    return os;
}

template <OrderBook B>
SymbolSnapshot snapshot(const B& b, Locate l) {
    SymbolSnapshot s;
    s.bids = levels(b, l, Side::Buy);
    s.asks = levels(b, l, Side::Sell);
    for (const auto& lv : s.bids) {
        s.bid_queues.push_back(orders_at(b, l, Side::Buy, lv.price));
    }
    for (const auto& lv : s.asks) {
        s.ask_queues.push_back(orders_at(b, l, Side::Sell, lv.price));
    }
    return s;
}

} // namespace tickpath::test
