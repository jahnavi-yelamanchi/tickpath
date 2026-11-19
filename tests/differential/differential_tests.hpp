#pragma once

// Typed differential suite: any OrderBook against ReferenceBook.

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "tickpath/sim/fault_injector.hpp"
#include "tickpath/sim/generator.hpp"

#include "support/differential.hpp"

namespace tickpath::test {

template <class B>
class Differential : public ::testing::Test {
protected:
    static void check(const std::vector<itch::AnyMessage>& msgs) {
        const auto d = run_differential<B>(msgs);
        EXPECT_FALSE(d.has_value()) << d->what;
    }
};

TYPED_TEST_SUITE_P(Differential);

TYPED_TEST_P(Differential, EdgeCases) {
    TestFixture::check(sim::generate_edge_cases());
}

TYPED_TEST_P(Differential, ContractViolations) {
    TestFixture::check(sim::generate_contract_violations());
}

TYPED_TEST_P(Differential, RandomDays) {
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        sim::GeneratorConfig cfg;
        cfg.seed = seed;
        cfg.book_messages = 5'000;
        cfg.symbols = static_cast<std::uint16_t>(1 + seed % 5);
        cfg.max_live_orders_per_symbol = seed % 2 == 0 ? 50 : 1'000;
        SCOPED_TRACE("seed " + std::to_string(seed));
        TestFixture::check(sim::generate_random(cfg));
    }
}

TYPED_TEST_P(Differential, DeepBookFewSymbols) {
    sim::GeneratorConfig cfg;
    cfg.seed = 77;
    cfg.book_messages = 20'000;
    cfg.symbols = 1;
    cfg.depth_ticks = 2'000;
    cfg.max_live_orders_per_symbol = 5'000;
    TestFixture::check(sim::generate_random(cfg));
}

TYPED_TEST_P(Differential, FaultedFeeds) {
    // Drops, duplicates and reordering produce unknown ids, duplicate ids and
    // overfills; the book must handle them exactly as the oracle does.
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        sim::GeneratorConfig cfg;
        cfg.seed = seed;
        cfg.book_messages = 5'000;
        cfg.symbols = 3;
        const auto clean = sim::sequence(sim::generate_random(cfg));
        const sim::FaultConfig faults{.seed = seed,
                                      .drop_ppm = 20'000,
                                      .duplicate_ppm = 20'000,
                                      .reorder_ppm = 20'000,
                                      .max_reorder_distance = 8};
        SCOPED_TRACE("seed " + std::to_string(seed));
        TestFixture::check(sim::messages_of(sim::inject_faults(clean, faults)));
    }
}

REGISTER_TYPED_TEST_SUITE_P(Differential, EdgeCases, ContractViolations, RandomDays,
                            DeepBookFewSymbols, FaultedFeeds);

} // namespace tickpath::test
