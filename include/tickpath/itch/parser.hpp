#pragma once

// ITCH 5.0 parser: wire bytes -> decoded messages -> handler.on(msg).
//
// OWNER-WRITTEN HOT PATH. The bodies below are stubs that return
// NotImplemented. Interface and semantics are fixed by this header and by
// tests/itch/test_parser.cpp; the implementation is the owner's.
//
// Requirements on the implementation:
//   - noexcept, no allocation (hot-path guard), no undefined behaviour on any
//     input (fuzzed), no reads outside the given span.
//   - Integers are big-endian on the wire (docs/VERSIONS.md); field offsets
//     are the `Offset` constants in messages.hpp.

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <variant>

#include "tickpath/itch/messages.hpp"

namespace tickpath::itch {

enum class ParseStatus : std::uint8_t {
    Ok,
    Truncated,      ///< fewer bytes than the message (or frame) needs
    UnknownType,    ///< type byte is not one of the 23 ITCH 5.0 types
    LengthMismatch, ///< more bytes than the type's length, or a frame length != the type's length
    InvalidField,   ///< buy/sell indicator not 'B' or 'S' (A, F, P)
    NotImplemented, ///< stub
};

namespace detail {
template <class H, class... Ts>
constexpr bool handles_all(std::variant<Ts...>* /*tag*/) noexcept {
    return (requires(H& h, const Ts& m) {
        { h.on(m) } noexcept;
    } && ...);
}
} // namespace detail

/// A parser callback: `on(const T&) noexcept` for every message type T.
template <class H>
concept Handler = detail::handles_all<H>(static_cast<AnyMessage*>(nullptr));

/// Decodes exactly one message (no length prefix) and calls handler.on()
/// once on success. On any error the handler is not called.
///   - empty span                          -> Truncated
///   - unknown type byte                   -> UnknownType
///   - size < length of the type           -> Truncated
///   - size > length of the type           -> LengthMismatch
///   - side byte not 'B'/'S' in A, F, P    -> InvalidField
template <Handler H>
[[nodiscard]] ParseStatus parse_message(std::span<const std::byte> msg, H& handler) noexcept {
    // OWNER: implement
    static_cast<void>(msg);
    static_cast<void>(handler);
    return ParseStatus::NotImplemented;
}

struct FrameResult {
    ParseStatus status{ParseStatus::Ok};
    std::size_t consumed{}; ///< bytes of complete frames processed (resume offset)
    std::size_t messages{}; ///< frames delivered to the handler
    std::size_t skipped{};  ///< frames with an unknown type, skipped by length
    friend bool operator==(const FrameResult&, const FrameResult&) = default;
};

/// Decodes consecutive frames `[u16 big-endian length][message]`.
///   - Stops with Ok when the buffer ends exactly on a frame boundary.
///   - Stops with Truncated at an incomplete trailing frame; `consumed` is the
///     offset of that frame, so a caller can append bytes and resume there.
///   - Skips a frame whose type is unknown (forward compatibility), counted in
///     `skipped`.
///   - Stops with LengthMismatch if a known type's frame length differs from
///     the spec length, or a frame length is 0; `consumed` is the offset of
///     that frame.
///   - Stops with InvalidField on a bad field; `consumed` is that frame's offset.
template <Handler H>
[[nodiscard]] FrameResult parse_frames(std::span<const std::byte> buf, H& handler) noexcept {
    // OWNER: implement
    static_cast<void>(buf);
    static_cast<void>(handler);
    return FrameResult{
        .status = ParseStatus::NotImplemented, .consumed = 0, .messages = 0, .skipped = 0};
}

[[nodiscard]] constexpr const char* to_string(ParseStatus s) noexcept {
    switch (s) {
    case ParseStatus::Ok:
        return "Ok";
    case ParseStatus::Truncated:
        return "Truncated";
    case ParseStatus::UnknownType:
        return "UnknownType";
    case ParseStatus::LengthMismatch:
        return "LengthMismatch";
    case ParseStatus::InvalidField:
        return "InvalidField";
    case ParseStatus::NotImplemented:
        return "NotImplemented";
    }
    return "?";
}

} // namespace tickpath::itch
