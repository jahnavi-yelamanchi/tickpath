#pragma once

// Reference order book: the oracle. std::map everywhere, no caching, level
// aggregates recomputed from orders on every query. Slow on purpose: every
// line should be obviously correct. Allocates; not for the hot path.

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <vector>

#include "tickpath/book/concept.hpp"
#include "tickpath/book/types.hpp"

namespace tickpath::book {

class ReferenceBook {
public:
    explicit ReferenceBook(const BookConfig& /*cfg*/ = {}) {}

    Status add(Locate l, OrderId id, Side s, Qty q, Price p) noexcept {
        if (q == 0) {
            return Status::ZeroQuantity;
        }
        if (orders_.contains(id)) {
            return Status::DuplicateOrder;
        }
        orders_[id] = Order{.id = id, .locate = l, .side = s, .price = p, .qty = q};
        queue(l, s, p).push_back(id);
        return Status::Ok;
    }

    Status execute(Locate l, OrderId id, Qty q) noexcept { return reduce(l, id, q); }

    Status cancel(Locate l, OrderId id, Qty q) noexcept { return reduce(l, id, q); }

    Status remove(Locate l, OrderId id) noexcept {
        const auto it = orders_.find(id);
        if (it == orders_.end()) {
            return Status::UnknownOrder;
        }
        if (it->second.locate != l) {
            return Status::LocateMismatch;
        }
        erase(it->second);
        return Status::Ok;
    }

    Status replace(Locate l, OrderId old_id, OrderId new_id, Qty q, Price p) noexcept {
        const auto it = orders_.find(old_id);
        if (it == orders_.end()) {
            return Status::UnknownOrder;
        }
        if (it->second.locate != l) {
            return Status::LocateMismatch;
        }
        if (q == 0) {
            return Status::ZeroQuantity;
        }
        if (orders_.contains(new_id)) { // includes new_id == old_id
            return Status::DuplicateOrder;
        }
        const Side side = it->second.side;
        erase(it->second);
        return add(l, new_id, side, q, p);
    }

    [[nodiscard]] std::optional<Level> best(Locate l, Side s) const noexcept {
        const auto* side = levels(l, s);
        if (side == nullptr) {
            return std::nullopt;
        }
        // Bids: highest price is the last key; asks: lowest is the first.
        const auto& entry = s == Side::Buy ? *side->rbegin() : *side->begin();
        return aggregate(entry.first, entry.second);
    }

    [[nodiscard]] std::optional<Order> find(OrderId id) const noexcept {
        const auto it = orders_.find(id);
        if (it == orders_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::size_t order_count() const noexcept { return orders_.size(); }

    template <class F>
    void for_each_level(Locate l, Side s, F fn) const noexcept {
        const auto* side = levels(l, s);
        if (side == nullptr) {
            return;
        }
        const auto visit = [&](const auto& entry) { fn(aggregate(entry.first, entry.second)); };
        if (s == Side::Buy) {
            std::for_each(side->rbegin(), side->rend(), visit);
        } else {
            std::for_each(side->begin(), side->end(), visit);
        }
    }

    template <class F>
    void for_each_order(Locate l, Side s, Price p, F fn) const noexcept {
        const auto* side = levels(l, s);
        if (side == nullptr) {
            return;
        }
        const auto it = side->find(p);
        if (it == side->end()) {
            return;
        }
        for (const OrderId id : it->second) {
            fn(orders_.at(id));
        }
    }

private:
    using Queue = std::vector<OrderId>;        // FIFO, front = oldest
    using SideLevels = std::map<Price, Queue>; // ascending price
    using Key = std::pair<Locate, Side>;

    [[nodiscard]] Level aggregate(Price p, const Queue& q) const {
        Level lv{.price = p, .qty = 0, .orders = 0};
        for (const OrderId id : q) {
            lv.qty += orders_.at(id).qty;
            ++lv.orders;
        }
        return lv;
    }

    Status reduce(Locate l, OrderId id, Qty q) noexcept {
        const auto it = orders_.find(id);
        if (it == orders_.end()) {
            return Status::UnknownOrder;
        }
        Order& o = it->second;
        if (o.locate != l) {
            return Status::LocateMismatch;
        }
        if (q == 0) {
            return Status::ZeroQuantity;
        }
        if (q > o.qty) {
            erase(o);
            return Status::Overfill;
        }
        o.qty -= q;
        if (o.qty == 0) {
            erase(o);
        }
        return Status::Ok;
    }

    Queue& queue(Locate l, Side s, Price p) { return book_[Key{l, s}][p]; }

    [[nodiscard]] const SideLevels* levels(Locate l, Side s) const noexcept {
        const auto it = book_.find(Key{l, s});
        return it == book_.end() ? nullptr : &it->second;
    }

    void erase(const Order& o) {
        const Order copy = o; // `o` refers into orders_
        auto& side = book_.at(Key{copy.locate, copy.side});
        auto& q = side.at(copy.price);
        q.erase(std::find(q.begin(), q.end(), copy.id));
        if (q.empty()) {
            side.erase(copy.price);
        }
        if (side.empty()) {
            book_.erase(Key{copy.locate, copy.side});
        }
        orders_.erase(copy.id);
    }

    std::map<OrderId, Order> orders_;
    std::map<Key, SideLevels> book_;
};

static_assert(OrderBook<ReferenceBook>);

} // namespace tickpath::book
