#pragma once

// Primitive field types of Nasdaq TotalView-ITCH 5.0.
// Wire format: big-endian, fixed-width fields, alpha fields left-justified
// and space-padded. Decoded values are host-order integers.

#include <array>
#include <cstdint>

namespace tickpath::itch {

using Locate = std::uint16_t;         ///< Stock Locate, 2 bytes. 0 is used for non-stock messages.
using TrackingNumber = std::uint16_t; ///< Nasdaq internal tracking number, 2 bytes.
using Timestamp = std::uint64_t;      ///< Nanoseconds since midnight, 6 bytes on the wire.
using OrderRef = std::uint64_t;       ///< Order Reference Number, 8 bytes, unique per day.
using MatchNumber = std::uint64_t;    ///< Match Number, 8 bytes.
using Shares = std::uint32_t;         ///< Share quantity, 4 bytes.
using Price4 = std::uint32_t;         ///< Price(4): 4 implied decimal places.
using Price8 = std::uint64_t;         ///< Price(8): 8 implied decimal places (MWCB levels only).

using Stock = std::array<char, 8>; ///< Alpha(8), space-padded.
using Mpid = std::array<char, 4>;  ///< Alpha(4), space-padded.

inline constexpr Timestamp max_timestamp = (Timestamp{1} << 48U) - 1U;

/// Largest Price(4) Nasdaq sends: 200,000.0000 (spec, Data Types). The wire
/// field can hold up to 2^32-1; values above this only arrive from corrupt
/// input, which the book contract still requires to be handled.
inline constexpr Price4 max_price4 = 2'000'000'000;

enum class Side : char {
    Buy = 'B',
    Sell = 'S',
};

/// Fields common to every message, after the 1-byte message type.
struct Header {
    Locate stock_locate{};
    TrackingNumber tracking_number{};
    Timestamp timestamp{};

    friend bool operator==(const Header&, const Header&) = default;
};

/// Builds a space-padded alpha field from a string literal (tests, generator).
template <std::size_t N, std::size_t M>
constexpr std::array<char, N> alpha(const char (&s)[M]) noexcept {
    static_assert(M - 1 <= N, "alpha value longer than field");
    std::array<char, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        out[i] = i + 1 < M ? s[i] : ' ';
    }
    return out;
}

} // namespace tickpath::itch
