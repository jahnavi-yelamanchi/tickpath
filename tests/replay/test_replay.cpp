// Deterministic replay with the oracle and typed messages (no parser).
// Must pass.

#include <cstdint>

#include <gtest/gtest.h>

#include "tickpath/book/feed_adapter.hpp"
#include "tickpath/book/reference_book.hpp"
#include "tickpath/book/state_hash.hpp"
#include "tickpath/sim/generator.hpp"

#include "replay/golden.hpp"

namespace tickpath::test {

namespace {

std::uint64_t replay_hash(const std::vector<itch::AnyMessage>& msgs) {
    book::ReferenceBook b;
    for (const auto& m : msgs) {
        static_cast<void>(book::apply(b, m));
    }
    return book::state_hash(b);
}

} // namespace

TEST(Replay, SameFeedSameHash) {
    const auto msgs = sim::generate_random(golden_feed_config());
    EXPECT_EQ(replay_hash(msgs), replay_hash(msgs));
}

TEST(Replay, OracleGoldenHash) {
    EXPECT_EQ(replay_hash(sim::generate_random(golden_feed_config())), golden_oracle_hash);
}

TEST(Replay, DifferentFeedDifferentHash) {
    auto cfg = golden_feed_config();
    cfg.seed += 1;
    EXPECT_NE(replay_hash(sim::generate_random(cfg)), golden_oracle_hash);
}

} // namespace tickpath::test
