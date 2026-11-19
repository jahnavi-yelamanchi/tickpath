#pragma once

// Differential runner: applies the same messages to a book under test and to
// the oracle and compares them.
//
// After every message: the status; every price level the message touched
// (FIFO queue and aggregate); best bid and offer of every touched symbol; the
// touched orders; the live order count.
// Every `full_every` messages: the full state of every touched symbol.
// At the end: the full-market state hash.
// Rationale and cost trade-off: docs/ADR-017.md.

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "tickpath/book/feed_adapter.hpp"
#include "tickpath/book/reference_book.hpp"
#include "tickpath/book/state_hash.hpp"
#include "tickpath/itch/messages.hpp"

#include "support/book_snapshot.hpp"

namespace tickpath::test {

inline constexpr book::BookConfig differential_config{.max_orders = 1U << 20U,
                                                      .max_levels = 1U << 18U};

/// Order ids referenced by a message (0, 1 or 2).
inline std::vector<book::OrderId> ids_of(const itch::AnyMessage& m) {
    return std::visit(
        [](const auto& x) -> std::vector<book::OrderId> {
            using M = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<M, itch::OrderReplace>) {
                return {x.original_order_reference_number, x.new_order_reference_number};
            } else if constexpr (book::is_order_message_v<M>) {
                return {x.order_reference_number};
            } else {
                return {};
            }
        },
        m);
}

/// (locate, side, price) of every level a message can change, from the
/// oracle's state before the message.
inline std::vector<std::tuple<Locate, Side, Price>> touched_levels(const book::ReferenceBook& ref,
                                                                   const itch::AnyMessage& m) {
    std::vector<std::tuple<Locate, Side, Price>> out;
    const Locate l = itch::header_of(m).stock_locate;
    for (const auto id : ids_of(m)) {
        if (const auto o = ref.find(id)) {
            out.emplace_back(o->locate, o->side, o->price);
            if (const auto* u = std::get_if<itch::OrderReplace>(&m)) {
                out.emplace_back(l, o->side, u->price);
            }
        }
    }
    if (const auto* a = std::get_if<itch::AddOrder>(&m)) {
        out.emplace_back(l, a->side, a->price);
    } else if (const auto* f = std::get_if<itch::AddOrderMpid>(&m)) {
        out.emplace_back(l, f->side, f->price);
    }
    return out;
}

/// Aggregate of one level as the book reports it through for_each_level.
template <book::OrderBook B>
std::optional<Level> reported_level(const B& b, Locate l, Side s, Price p) {
    std::optional<Level> out;
    b.for_each_level(l, s, [&](const Level& lv) {
        if (lv.price == p) {
            out = lv;
        }
    });
    return out;
}

/// Aggregate of one level by definition: computed from its orders.
inline std::optional<Level> defined_level(const std::vector<Order>& q, Price p) {
    if (q.empty()) {
        return std::nullopt;
    }
    Level lv{.price = p, .qty = 0, .orders = 0};
    for (const auto& o : q) {
        lv.qty += o.qty;
        ++lv.orders;
    }
    return lv;
}

struct Divergence {
    std::size_t index{};
    std::string what;
};

template <book::OrderBook B>
std::optional<Divergence> run_differential(std::span<const itch::AnyMessage> msgs,
                                           const book::BookConfig& cfg = differential_config,
                                           std::size_t full_every = 64) {
    book::ReferenceBook ref;
    auto sut = std::make_unique<B>(cfg);
    for (std::size_t i = 0; i < msgs.size(); ++i) {
        const auto& m = msgs[i];
        const auto ids = ids_of(m);
        const auto lvls = touched_levels(ref, m);
        std::vector<Locate> locs{itch::header_of(m).stock_locate};
        for (const auto& t : lvls) {
            locs.push_back(std::get<0>(t));
        }

        const auto want = book::apply(ref, m);
        const auto got = book::apply(*sut, m);

        const auto fail = [&](const std::string& what) {
            std::ostringstream os;
            os << "message " << i << " type '" << itch::type_of(m) << "' locate "
               << itch::header_of(m).stock_locate << ": " << what;
            return Divergence{.index = i, .what = os.str()};
        };
        if (want != got) {
            std::ostringstream os;
            os << "status: oracle " << (want ? book::to_string(*want) : "none") << ", book "
               << (got ? book::to_string(*got) : "none");
            return fail(os.str());
        }
        if (!want) {
            continue;
        }
        for (const auto& [l, s, p] : lvls) {
            const auto qa = orders_at(ref, l, s, p);
            const auto qb = orders_at(*sut, l, s, p);
            if (qa != qb) {
                std::ostringstream os;
                os << "queue at locate " << l << " " << static_cast<char>(s) << " " << p
                   << ": oracle " << qa.size() << " orders, book " << qb.size();
                return fail(os.str());
            }
            if (defined_level(qa, p) != reported_level(*sut, l, s, p)) {
                std::ostringstream os;
                os << "level aggregate at locate " << l << " " << static_cast<char>(s) << " " << p;
                return fail(os.str());
            }
        }
        for (const Locate l : locs) {
            for (const Side s : {Side::Buy, Side::Sell}) {
                if (ref.best(l, s) != sut->best(l, s)) {
                    return fail("best " + std::string(1, static_cast<char>(s)) + " at locate " +
                                std::to_string(l));
                }
            }
            if (i % full_every == 0) {
                const auto a = snapshot(ref, l);
                const auto b = snapshot(*sut, l);
                if (a != b) {
                    std::ostringstream os;
                    os << "symbol " << l << " state\n  oracle:\n" << a << "  book:\n" << b;
                    return fail(os.str());
                }
            }
        }
        for (const auto id : ids) {
            if (ref.find(id) != sut->find(id)) {
                return fail("find(" + std::to_string(id) + ") differs");
            }
        }
        if (ref.order_count() != sut->order_count()) {
            return fail("order_count: oracle " + std::to_string(ref.order_count()) + ", book " +
                        std::to_string(sut->order_count()));
        }
    }
    if (book::state_hash(ref) != book::state_hash(*sut)) {
        return Divergence{.index = msgs.size(), .what = "final state hash differs"};
    }
    return std::nullopt;
}

} // namespace tickpath::test
