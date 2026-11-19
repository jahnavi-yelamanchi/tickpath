#pragma once

// Random book operations for property tests. Small id, locate and price
// domains so that collisions (duplicates, unknown ids, shared levels,
// mismatched locates, overfills) are frequent.

#include <cstdint>
#include <ostream>
#include <rapidcheck.h>
#include <vector>

#include "tickpath/book/reference_book.hpp"

#include "support/book_snapshot.hpp"

namespace tickpath::test {

struct Op {
    enum Kind : std::uint8_t { Add, Execute, Cancel, Remove, Replace } kind{Add};
    Locate locate{};
    book::OrderId id{};
    book::OrderId new_id{};
    Side side{Side::Buy};
    book::Qty qty{};
    Price price{};
};

inline std::ostream& operator<<(std::ostream& os, const Op& o) {
    static constexpr const char* names[] = {"add", "execute", "cancel", "remove", "replace"};
    return os << names[o.kind] << "(l=" << o.locate << " id=" << o.id << " new=" << o.new_id << " "
              << static_cast<char>(o.side) << " q=" << o.qty << " p=" << o.price << ")";
}

inline rc::Gen<Op> gen_op() {
    return rc::gen::apply(
        [](int kind, Locate l, book::OrderId id, book::OrderId new_id, bool buy, book::Qty q,
           int tick) {
            return Op{.kind = static_cast<Op::Kind>(kind),
                      .locate = l,
                      .id = id,
                      .new_id = new_id,
                      .side = buy ? Side::Buy : Side::Sell,
                      .qty = q,
                      .price = static_cast<Price>(100'000 + 100 * tick)};
        },
        rc::gen::weightedElement<int>(
            {{5, Op::Add}, {2, Op::Execute}, {2, Op::Cancel}, {2, Op::Remove}, {2, Op::Replace}}),
        rc::gen::inRange<Locate>(1, 4), rc::gen::inRange<book::OrderId>(1, 40),
        rc::gen::inRange<book::OrderId>(1, 40), rc::gen::arbitrary<bool>(),
        rc::gen::weightedOneOf<book::Qty>(
            {{1, rc::gen::just<book::Qty>(0)}, {10, rc::gen::inRange<book::Qty>(1, 300)}}),
        rc::gen::inRange(-6, 7));
}

template <book::OrderBook B>
book::Status apply_op(B& b, const Op& o) {
    switch (o.kind) {
    case Op::Add:
        return b.add(o.locate, o.id, o.side, o.qty, o.price);
    case Op::Execute:
        return b.execute(o.locate, o.id, o.qty);
    case Op::Cancel:
        return b.cancel(o.locate, o.id, o.qty);
    case Op::Remove:
        return b.remove(o.locate, o.id);
    case Op::Replace:
        return b.replace(o.locate, o.id, o.new_id, o.qty, o.price);
    }
    return book::Status::NotImplemented;
}

inline constexpr Locate prop_locates[] = {1, 2, 3};

/// Book invariants that hold for any correct implementation.
template <book::OrderBook B>
void check_invariants(const B& b) {
    std::size_t orders = 0;
    for (const Locate l : prop_locates) {
        for (const Side s : {Side::Buy, Side::Sell}) {
            const auto lvls = levels(b, l, s);
            for (std::size_t i = 0; i < lvls.size(); ++i) {
                const auto& lv = lvls[i];
                RC_ASSERT(lv.orders > 0U);
                if (i > 0) {
                    const bool sorted = s == Side::Buy ? lvls[i - 1].price > lv.price
                                                       : lvls[i - 1].price < lv.price;
                    RC_ASSERT(sorted);
                }
                std::uint64_t qty = 0;
                const auto q = orders_at(b, l, s, lv.price);
                RC_ASSERT(q.size() == lv.orders);
                for (const auto& o : q) {
                    RC_ASSERT(o.qty > 0U);
                    RC_ASSERT(o.locate == l);
                    RC_ASSERT(o.side == s);
                    RC_ASSERT(o.price == lv.price);
                    RC_ASSERT((b.find(o.id) == std::optional<Order>{o}));
                    qty += o.qty;
                }
                RC_ASSERT(qty == lv.qty);
                orders += q.size();
            }
            const auto best = b.best(l, s);
            RC_ASSERT((best.has_value() == !lvls.empty()));
            if (best) {
                RC_ASSERT(*best == lvls.front());
            }
        }
    }
    RC_ASSERT(orders == b.order_count());
}

/// Book under test and oracle agree on every status and on full state.
template <book::OrderBook B>
void check_against_oracle(const std::vector<Op>& ops) {
    book::ReferenceBook ref;
    B sut(book::BookConfig{.max_orders = 1U << 12U, .max_levels = 1U << 12U});
    for (const auto& op : ops) {
        RC_ASSERT(apply_op(sut, op) == apply_op(ref, op));
        for (const Locate l : prop_locates) {
            RC_ASSERT(snapshot(sut, l) == snapshot(ref, l));
        }
        RC_ASSERT(sut.order_count() == ref.order_count());
    }
    check_invariants(sut);
}

} // namespace tickpath::test
