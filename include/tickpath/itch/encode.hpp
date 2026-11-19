#pragma once

// ITCH 5.0 encoder: decoded message -> big-endian wire bytes.
// Used by the synthetic generator and by tests as the oracle for the parser.
// Not on the hot path; it may throw.

#include <cstddef>
#include <span>
#include <vector>

#include "tickpath/itch/messages.hpp"

namespace tickpath::itch {

/// Size of the length prefix in the Nasdaq sample-file framing.
inline constexpr std::size_t frame_prefix_size = 2;

/// Writes one message without a length prefix. Returns bytes written.
/// Throws std::length_error if `out` is too small and std::out_of_range if a
/// 6-byte timestamp does not fit in 48 bits.
std::size_t encode(const AnyMessage& msg, std::span<std::byte> out);

/// Appends one frame: 2-byte big-endian length, then the message.
void append_framed(const AnyMessage& msg, std::vector<std::byte>& out);

/// Encodes a sequence as consecutive frames.
[[nodiscard]] std::vector<std::byte> encode_framed(std::span<const AnyMessage> msgs);

} // namespace tickpath::itch
