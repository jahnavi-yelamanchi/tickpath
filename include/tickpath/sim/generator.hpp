#pragma once

// Synthetic ITCH 5.0 feed generator. Deterministic for a given config on any
// platform (see rng.hpp). Not on the hot path.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tickpath/itch/messages.hpp"

namespace tickpath::sim {

struct GeneratorConfig {
    std::uint64_t seed = 1;
    std::size_t book_messages = 10'000; ///< order-flow messages after the open
    std::uint16_t symbols = 8;          ///< locates 1..symbols
    std::size_t max_live_orders_per_symbol = 500;
    std::uint32_t tick = 100;       ///< Price(4) units; 100 = $0.01
    std::uint32_t depth_ticks = 50; ///< max distance of a quote from mid, in ticks
};

/// A full trading day: system events, directory and status messages, then
/// book-consistent random order flow (every execute, cancel, delete and
/// replace refers to a live order with enough shares; the book never crosses),
/// interleaved with trade, cross, imbalance and other non-book messages.
[[nodiscard]] std::vector<itch::AnyMessage> generate_random(const GeneratorConfig& cfg);

/// Small directed feed, valid ITCH only: every message type, partial and full
/// executions, executions at a different price, partial and full cancels,
/// deletes, replaces (same price, new price, larger size), FIFO at one level,
/// levels emptying, extreme prices, locate 65535, order ref 0 and 2^64-1.
[[nodiscard]] std::vector<itch::AnyMessage> generate_edge_cases();

/// Small directed feed of inputs a correct exchange never sends but the book
/// contract defines (docs/BOOK_CONTRACT.md): unknown ids, zero quantities,
/// overfills, duplicate ids, replace onto a live id, locate mismatch, crossed
/// adds and id reuse after removal.
[[nodiscard]] std::vector<itch::AnyMessage> generate_contract_violations();

} // namespace tickpath::sim
