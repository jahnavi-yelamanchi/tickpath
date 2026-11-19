#pragma once

// Portable deterministic RNG. std::uniform_int_distribution is not specified
// bit-for-bit, so feeds generated with it differ between standard libraries.
// This one is: splitmix64 seeding, xoshiro256** output, rejection sampling.

#include <array>
#include <bit>
#include <cstdint>

namespace tickpath::sim {

class Rng {
public:
    explicit constexpr Rng(std::uint64_t seed) noexcept {
        for (auto& s : state_) {
            s = splitmix64(seed);
        }
    }

    constexpr std::uint64_t next() noexcept {
        const std::uint64_t result = std::rotl(state_[1] * 5U, 7) * 9U;
        const std::uint64_t t = state_[1] << 17U;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = std::rotl(state_[3], 45);
        return result;
    }

    /// Uniform in [0, n). n must be > 0.
    constexpr std::uint64_t below(std::uint64_t n) noexcept {
        const std::uint64_t threshold = (0U - n) % n;
        for (;;) {
            const std::uint64_t r = next();
            if (r >= threshold) {
                return r % n;
            }
        }
    }

    /// Uniform in [lo, hi].
    constexpr std::uint64_t between(std::uint64_t lo, std::uint64_t hi) noexcept {
        return lo + below(hi - lo + 1U);
    }

    /// True with probability num/den.
    constexpr bool chance(std::uint64_t num, std::uint64_t den) noexcept {
        return below(den) < num;
    }

private:
    static constexpr std::uint64_t splitmix64(std::uint64_t& x) noexcept {
        x += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = x;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    std::array<std::uint64_t, 4> state_{};
};

} // namespace tickpath::sim
