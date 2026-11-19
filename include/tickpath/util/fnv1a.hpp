#pragma once

// FNV-1a 64-bit. Stable across platforms; used for determinism checks.

#include <cstddef>
#include <cstdint>
#include <span>

namespace tickpath::util {

class Fnv1a {
public:
    static constexpr std::uint64_t offset_basis = 0xCBF29CE484222325ULL;
    static constexpr std::uint64_t prime = 0x100000001B3ULL;

    constexpr void byte(std::uint8_t b) noexcept {
        h_ ^= b;
        h_ *= prime;
    }

    constexpr void bytes(std::span<const std::byte> s) noexcept {
        for (const std::byte b : s) {
            byte(static_cast<std::uint8_t>(b));
        }
    }

    /// Mixes an integer as little-endian bytes of its width (platform independent).
    template <class T>
    constexpr void integer(T v) noexcept {
        auto u = static_cast<std::uint64_t>(v);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            byte(static_cast<std::uint8_t>(u & 0xFFU));
            u >>= 8U;
        }
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return h_; }

private:
    std::uint64_t h_ = offset_basis;
};

} // namespace tickpath::util
