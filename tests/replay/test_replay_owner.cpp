// OWNER: full pipeline replay, bytes -> parser -> book, for every book. The
// final state hash must equal the oracle's golden hash and be identical
// across runs.

#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "tickpath/book/flat_book.hpp"
#include "tickpath/book/reference_book.hpp"
#include "tickpath/book/replay.hpp"
#include "tickpath/book/state_hash.hpp"
#include "tickpath/book/tree_book.hpp"
#include "tickpath/itch/encode.hpp"

#include "replay/golden.hpp"

namespace tickpath::test {

template <class B>
class PipelineReplay : public ::testing::Test {};

using Books = ::testing::Types<book::ReferenceBook, book::FlatBook, book::TreeBook>;
TYPED_TEST_SUITE(PipelineReplay, Books);

TYPED_TEST(PipelineReplay, MatchesGoldenAcrossRuns) {
    const auto msgs = sim::generate_random(golden_feed_config());
    const auto bytes = itch::encode_framed(msgs);
    std::uint64_t first = 0;
    for (int run = 0; run < 2; ++run) {
        auto b = std::make_unique<TypeParam>(book::BookConfig{});
        const auto r = book::replay(bytes, *b);
        ASSERT_EQ(r.frames.status, itch::ParseStatus::Ok);
        ASSERT_EQ(r.frames.messages, msgs.size());
        const auto h = book::state_hash(*b);
        if (run == 0) {
            first = h;
        } else {
            EXPECT_EQ(h, first) << "replay is not deterministic";
        }
        EXPECT_EQ(h, golden_oracle_hash);
    }
}

TYPED_TEST(PipelineReplay, ValidFeedHasOnlyOkStatuses) {
    const auto bytes = itch::encode_framed(sim::generate_random(golden_feed_config()));
    auto b = std::make_unique<TypeParam>(book::BookConfig{});
    const auto r = book::replay(bytes, *b);
    ASSERT_EQ(r.frames.status, itch::ParseStatus::Ok);
    std::uint64_t not_ok = 0;
    for (std::size_t i = 1; i < r.statuses.size(); ++i) {
        not_ok += r.statuses[i];
    }
    EXPECT_GT(r.statuses[0], 0U);
    EXPECT_EQ(not_ok, 0U);
}

} // namespace tickpath::test
