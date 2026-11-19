#pragma once

// Fault injection for gap handling: drop, duplicate and reorder messages of a
// sequenced stream. Sequence numbers are assigned 1..n, one per message, as
// MoldUDP64 would (ADR-006).

#include <cstdint>
#include <span>
#include <vector>

#include "tickpath/itch/messages.hpp"

namespace tickpath::sim {

struct Sequenced {
    std::uint64_t seq{};
    itch::AnyMessage msg;
};

struct FaultConfig {
    std::uint64_t seed = 1;
    std::uint32_t drop_ppm = 0;             ///< per-million chance a message is dropped
    std::uint32_t duplicate_ppm = 0;        ///< per-million chance a message is sent twice
    std::uint32_t reorder_ppm = 0;          ///< per-million chance a message is delayed
    std::uint32_t max_reorder_distance = 4; ///< how many later messages may overtake it
};

struct FaultStats {
    std::uint64_t dropped = 0;
    std::uint64_t duplicated = 0;
    std::uint64_t reordered = 0;
};

/// Assigns sequence numbers 1..n.
[[nodiscard]] std::vector<Sequenced> sequence(std::span<const itch::AnyMessage> msgs);

/// Applies faults. Deterministic for a given config. Original sequence
/// numbers are kept, so a consumer can detect the gap, the duplicate and the
/// late arrival.
[[nodiscard]] std::vector<Sequenced>
inject_faults(std::span<const Sequenced> in, const FaultConfig& cfg, FaultStats* stats = nullptr);

/// Strips sequence numbers.
[[nodiscard]] std::vector<itch::AnyMessage> messages_of(std::span<const Sequenced> in);

} // namespace tickpath::sim
