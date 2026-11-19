// Synthetic generator and fault injector.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "tickpath/itch/encode.hpp"
#include "tickpath/sim/fault_injector.hpp"
#include "tickpath/sim/generator.hpp"
#include "tickpath/sim/rng.hpp"
#include "tickpath/util/fnv1a.hpp"

namespace itch = tickpath::itch;
namespace sim = tickpath::sim;

namespace {

std::uint64_t hash_of(const std::vector<itch::AnyMessage>& msgs) {
    tickpath::util::Fnv1a h;
    h.bytes(itch::encode_framed(msgs));
    return h.value();
}

std::set<char> types_in(const std::vector<itch::AnyMessage>& msgs) {
    std::set<char> t;
    for (const auto& m : msgs) {
        t.insert(itch::type_of(m));
    }
    return t;
}

/// Replays order flow and reports the first message that is not valid ITCH:
/// unknown id, overfill, zero quantity, duplicate id or locate mismatch.
struct ValidityChecker {
    struct Order {
        itch::Locate locate;
        itch::Side side;
        itch::Price4 price;
        itch::Shares shares;
    };
    std::unordered_map<itch::OrderRef, Order> live;
    std::string error;

    bool reduce(itch::Locate l, itch::OrderRef id, itch::Shares qty) {
        const auto it = live.find(id);
        if (it == live.end() || it->second.locate != l || qty == 0 || qty > it->second.shares) {
            return false;
        }
        it->second.shares -= qty;
        if (it->second.shares == 0) {
            live.erase(it);
        }
        return true;
    }

    bool add(itch::Locate l, itch::OrderRef id, itch::Side s, itch::Shares q, itch::Price4 p) {
        return q > 0 && live.emplace(id, Order{l, s, p, q}).second;
    }

    bool apply(const itch::AnyMessage& m) {
        const auto l = itch::header_of(m).stock_locate;
        if (const auto* a = std::get_if<itch::AddOrder>(&m)) {
            return add(l, a->order_reference_number, a->side, a->shares, a->price);
        }
        if (const auto* f = std::get_if<itch::AddOrderMpid>(&m)) {
            return add(l, f->order_reference_number, f->side, f->shares, f->price);
        }
        if (const auto* e = std::get_if<itch::OrderExecuted>(&m)) {
            return reduce(l, e->order_reference_number, e->executed_shares);
        }
        if (const auto* c = std::get_if<itch::OrderExecutedWithPrice>(&m)) {
            return reduce(l, c->order_reference_number, c->executed_shares);
        }
        if (const auto* x = std::get_if<itch::OrderCancel>(&m)) {
            return reduce(l, x->order_reference_number, x->cancelled_shares);
        }
        if (const auto* d = std::get_if<itch::OrderDelete>(&m)) {
            const auto it = live.find(d->order_reference_number);
            if (it == live.end() || it->second.locate != l) {
                return false;
            }
            live.erase(it);
            return true;
        }
        if (const auto* u = std::get_if<itch::OrderReplace>(&m)) {
            const auto it = live.find(u->original_order_reference_number);
            if (it == live.end() || it->second.locate != l || u->shares == 0 ||
                live.contains(u->new_order_reference_number)) {
                return false;
            }
            const Order o = it->second;
            live.erase(it);
            return add(l, u->new_order_reference_number, o.side, u->shares, u->price);
        }
        return true;
    }
};

std::size_t first_invalid(const std::vector<itch::AnyMessage>& msgs) {
    ValidityChecker v;
    for (std::size_t i = 0; i < msgs.size(); ++i) {
        if (!v.apply(msgs[i])) {
            return i;
        }
    }
    return msgs.size();
}

} // namespace

// ---- RNG ---------------------------------------------------------------------

TEST(Rng, GoldenSequenceIsPortable) {
    // Pinned output; a change here changes every generated feed.
    sim::Rng r(1);
    const std::vector<std::uint64_t> got{r.next(), r.next(), r.next()};
    sim::Rng again(1);
    EXPECT_EQ(got, (std::vector<std::uint64_t>{again.next(), again.next(), again.next()}));
    EXPECT_EQ(got[0], 12966619160104079557ULL);
}

TEST(Rng, BelowStaysInRange) {
    sim::Rng r(7);
    for (int i = 0; i < 10'000; ++i) {
        EXPECT_LT(r.below(3), 3U);
        const auto v = r.between(5, 9);
        EXPECT_GE(v, 5U);
        EXPECT_LE(v, 9U);
    }
    EXPECT_EQ(r.below(1), 0U);
}

// ---- Random day --------------------------------------------------------------

TEST(Generator, SameSeedSameBytes) {
    sim::GeneratorConfig cfg;
    cfg.seed = 42;
    cfg.book_messages = 5'000;
    EXPECT_EQ(hash_of(sim::generate_random(cfg)), hash_of(sim::generate_random(cfg)));
    auto other = cfg;
    other.seed = 43;
    EXPECT_NE(hash_of(sim::generate_random(cfg)), hash_of(sim::generate_random(other)));
}

TEST(Generator, GoldenHash) {
    // Pinned: catches accidental changes to the generator or encoder, and
    // differences between platforms. Update deliberately, with the reason.
    sim::GeneratorConfig cfg;
    cfg.seed = 42;
    cfg.book_messages = 2'000;
    EXPECT_EQ(hash_of(sim::generate_random(cfg)), 4517631755464299224ULL);
}

TEST(Generator, RandomDayIsValidItch) {
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        sim::GeneratorConfig cfg;
        cfg.seed = seed;
        cfg.book_messages = 3'000;
        cfg.symbols = 4;
        cfg.max_live_orders_per_symbol = 50; // forces the delete path
        const auto msgs = sim::generate_random(cfg);
        EXPECT_EQ(first_invalid(msgs), msgs.size()) << "seed " << seed;
    }
}

TEST(Generator, RandomDayNeverCrosses) {
    sim::GeneratorConfig cfg;
    cfg.book_messages = 20'000;
    const auto msgs = sim::generate_random(cfg);
    std::map<itch::Locate, itch::Price4> max_bid;
    std::map<itch::Locate, itch::Price4> min_ask;
    const auto see = [&](itch::Locate l, itch::Side s, itch::Price4 p) {
        if (s == itch::Side::Buy) {
            max_bid[l] = std::max(max_bid[l], p);
        } else {
            min_ask.try_emplace(l, std::numeric_limits<itch::Price4>::max());
            min_ask[l] = std::min(min_ask[l], p);
        }
    };
    for (const auto& m : msgs) {
        if (const auto* a = std::get_if<itch::AddOrder>(&m)) {
            see(a->header.stock_locate, a->side, a->price);
        } else if (const auto* f = std::get_if<itch::AddOrderMpid>(&m)) {
            see(f->header.stock_locate, f->side, f->price);
        }
    }
    for (const auto& [l, bid] : max_bid) {
        EXPECT_LT(bid, min_ask[l]) << "locate " << l;
    }
}

TEST(Generator, TimestampsNonDecreasing) {
    for (const auto& msgs : {sim::generate_random({}), sim::generate_edge_cases(),
                             sim::generate_contract_violations()}) {
        for (std::size_t i = 1; i < msgs.size(); ++i) {
            EXPECT_LE(itch::header_of(msgs[i - 1]).timestamp, itch::header_of(msgs[i]).timestamp);
        }
    }
}

TEST(Generator, RandomDayHasBookAndNonBookTypes) {
    sim::GeneratorConfig cfg;
    cfg.book_messages = 20'000;
    const auto t = types_in(sim::generate_random(cfg));
    for (char c :
         {'S', 'R', 'H', 'Y', 'V', 'A', 'F', 'E', 'C', 'X', 'D', 'U', 'P', 'Q', 'I', 'N'}) {
        EXPECT_TRUE(t.contains(c)) << c;
    }
}

// ---- Directed scenarios -------------------------------------------------------

TEST(Generator, EdgeCasesCoverEveryMessageType) {
    EXPECT_EQ(types_in(sim::generate_edge_cases()).size(), itch::message_type_count);
}

TEST(Generator, EdgeCasesAreValidItch) {
    const auto msgs = sim::generate_edge_cases();
    EXPECT_EQ(first_invalid(msgs), msgs.size());
}

TEST(Generator, EdgeCasesContainExtremes) {
    const auto msgs = sim::generate_edge_cases();
    bool min_px = false;
    bool max_px = false;
    bool max_locate = false;
    bool max_ref = false;
    bool exec_other_price = false;
    for (const auto& m : msgs) {
        if (const auto* a = std::get_if<itch::AddOrder>(&m)) {
            min_px |= a->price == 1;
            max_px |= a->price == 2'000'000'000;
            max_locate |= a->header.stock_locate == 65'535;
            max_ref |= a->order_reference_number == std::numeric_limits<itch::OrderRef>::max();
        }
        if (const auto* c = std::get_if<itch::OrderExecutedWithPrice>(&m)) {
            exec_other_price |= c->execution_price != 100'000;
        }
    }
    EXPECT_TRUE(min_px);
    EXPECT_TRUE(max_px);
    EXPECT_TRUE(max_locate);
    EXPECT_TRUE(max_ref);
    EXPECT_TRUE(exec_other_price);
}

TEST(Generator, ContractViolationsAreInvalidItch) {
    const auto msgs = sim::generate_contract_violations();
    EXPECT_LT(first_invalid(msgs), msgs.size());
}

// ---- Fault injector ------------------------------------------------------------

TEST(FaultInjector, SequenceNumbersStartAtOne) {
    const auto s = sim::sequence(sim::generate_edge_cases());
    ASSERT_FALSE(s.empty());
    for (std::size_t i = 0; i < s.size(); ++i) {
        EXPECT_EQ(s[i].seq, i + 1);
    }
}

TEST(FaultInjector, NoFaultsIsIdentity) {
    const auto s = sim::sequence(sim::generate_random({}));
    sim::FaultStats st;
    const auto out = sim::inject_faults(s, {}, &st);
    ASSERT_EQ(out.size(), s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        EXPECT_EQ(out[i].seq, s[i].seq);
    }
    EXPECT_EQ(st.dropped + st.duplicated + st.reordered, 0U);
}

TEST(FaultInjector, DropAll) {
    const auto s = sim::sequence(sim::generate_edge_cases());
    const auto out = sim::inject_faults(s, {.seed = 1, .drop_ppm = 1'000'000});
    EXPECT_TRUE(out.empty());
}

TEST(FaultInjector, CountsMatchOutput) {
    const auto s = sim::sequence(sim::generate_random({}));
    sim::FaultStats st;
    const sim::FaultConfig cfg{.seed = 9,
                               .drop_ppm = 10'000,
                               .duplicate_ppm = 10'000,
                               .reorder_ppm = 10'000,
                               .max_reorder_distance = 4};
    const auto out = sim::inject_faults(s, cfg, &st);
    EXPECT_GT(st.dropped, 0U);
    EXPECT_GT(st.duplicated, 0U);
    EXPECT_GT(st.reordered, 0U);
    EXPECT_EQ(out.size(), s.size() - st.dropped + st.duplicated);

    // Every surviving seq appears once, or twice if duplicated; none invented.
    std::map<std::uint64_t, int> seen;
    for (const auto& x : out) {
        ++seen[x.seq];
    }
    std::uint64_t dup = 0;
    for (const auto& [seq, n] : seen) {
        EXPECT_GE(seq, 1U);
        EXPECT_LE(seq, s.size());
        EXPECT_LE(n, 2);
        dup += static_cast<std::uint64_t>(n - 1);
    }
    EXPECT_EQ(dup, st.duplicated);
    EXPECT_EQ(seen.size(), s.size() - st.dropped);

    // Out-of-order arrivals exist.
    bool backwards = false;
    for (std::size_t i = 1; i < out.size(); ++i) {
        backwards |= out[i].seq < out[i - 1].seq;
    }
    EXPECT_TRUE(backwards);
}

TEST(FaultInjector, Deterministic) {
    const auto s = sim::sequence(sim::generate_random({}));
    const sim::FaultConfig cfg{.seed = 3,
                               .drop_ppm = 5'000,
                               .duplicate_ppm = 5'000,
                               .reorder_ppm = 5'000,
                               .max_reorder_distance = 8};
    const auto a = sim::inject_faults(s, cfg);
    const auto b = sim::inject_faults(s, cfg);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].seq, b[i].seq);
    }
}
