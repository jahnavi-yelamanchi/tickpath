#pragma once

// Golden replay: final state hash of the oracle after replaying a fixed
// synthetic day. Every book, through the parser, must reproduce it.
// Update only with a deliberate generator, encoder or contract change.

#include <cstdint>

#include "tickpath/sim/generator.hpp"

namespace tickpath::test {

inline sim::GeneratorConfig golden_feed_config() {
    sim::GeneratorConfig cfg;
    cfg.seed = 20'261'119;
    cfg.book_messages = 50'000;
    cfg.symbols = 16;
    return cfg;
}

inline constexpr std::uint64_t golden_oracle_hash = 1488537752681063466ULL;

} // namespace tickpath::test
