#pragma once

// Replay helpers: a parser handler that applies messages to a book and counts
// book statuses. Used by tickpath-replay, the replay tests and the benchmark.

#include <array>
#include <cstdint>
#include <span>

#include "tickpath/book/feed_adapter.hpp"
#include "tickpath/itch/parser.hpp"

namespace tickpath::book {

inline constexpr std::size_t status_count = static_cast<std::size_t>(Status::NotImplemented) + 1U;

template <OrderBook B>
class ReplayHandler {
public:
    explicit ReplayHandler(B& book) noexcept
        : updater_(book) {}

    template <class M>
    void on(const M& m) noexcept {
        ++messages_;
        updater_.on(m);
        if (const auto s = updater_.last_status()) {
            ++statuses_[static_cast<std::size_t>(*s)];
        }
    }

    [[nodiscard]] std::uint64_t messages() const noexcept { return messages_; }
    [[nodiscard]] const std::array<std::uint64_t, status_count>& statuses() const noexcept {
        return statuses_;
    }

private:
    BookUpdater<B> updater_;
    std::uint64_t messages_ = 0;
    std::array<std::uint64_t, status_count> statuses_{};
};

struct ReplayResult {
    itch::FrameResult frames;
    std::array<std::uint64_t, status_count> statuses{};
};

/// Parses framed bytes into `book`.
template <OrderBook B>
ReplayResult replay(std::span<const std::byte> framed, B& book) noexcept {
    ReplayHandler<B> h(book);
    ReplayResult r;
    r.frames = itch::parse_frames(framed, h);
    r.statuses = h.statuses();
    return r;
}

} // namespace tickpath::book
