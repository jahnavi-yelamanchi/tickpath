#pragma once

// Ready-made parser handlers for tests, fuzzing and benchmarks.

#include <array>
#include <cstdint>
#include <vector>

#include "tickpath/itch/messages.hpp"

namespace tickpath::itch {

/// Counts messages per type byte. No allocation.
struct CountingHandler {
    std::array<std::uint64_t, 256> by_type{};
    std::uint64_t total = 0;

    template <class M>
    void on(const M& /*msg*/) noexcept {
        ++by_type[static_cast<unsigned char>(M::type)];
        ++total;
    }
};

/// Collects decoded messages. Allocates; tests only.
struct CollectingHandler {
    std::vector<AnyMessage> messages;

    template <class M>
    void on(const M& msg) noexcept {
        messages.emplace_back(msg); // may terminate on bad_alloc; acceptable in tests
    }
};

} // namespace tickpath::itch
