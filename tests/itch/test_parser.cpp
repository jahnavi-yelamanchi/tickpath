// OWNER: parser tests. The encoder (checked against golden bytes in
// test_messages.cpp) is the oracle: encode -> parse must give back the input.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "tickpath/itch/encode.hpp"
#include "tickpath/itch/handlers.hpp"
#include "tickpath/itch/parser.hpp"
#include "tickpath/sim/generator.hpp"

namespace itch = tickpath::itch;
using itch::ParseStatus;

namespace tickpath::itch {
inline std::ostream& operator<<(std::ostream& os, ParseStatus s) {
    return os << to_string(s);
}
inline std::ostream& operator<<(std::ostream& os, const FrameResult& r) {
    return os << "{" << to_string(r.status) << " consumed=" << r.consumed
              << " messages=" << r.messages << " skipped=" << r.skipped << "}";
}
} // namespace tickpath::itch

namespace {

std::vector<std::byte> encode_one(const itch::AnyMessage& m) {
    std::vector<std::byte> out(itch::max_message_size);
    out.resize(itch::encode(m, out));
    return out;
}

/// One message of every type, with the field values of the edge-case feed.
std::map<char, itch::AnyMessage> one_of_each() {
    std::map<char, itch::AnyMessage> out;
    for (const auto& m : tickpath::sim::generate_edge_cases()) {
        out.try_emplace(itch::type_of(m), m);
    }
    return out;
}

void put_be(std::vector<std::byte>& v, std::size_t off, std::uint64_t x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        v[off + i] = static_cast<std::byte>((x >> (8U * (n - 1U - i))) & 0xFFU);
    }
}

} // namespace

// ---- Single messages -----------------------------------------------------------

TEST(ParseMessage, RoundTripEveryType) {
    const auto all = one_of_each();
    ASSERT_EQ(all.size(), itch::message_type_count);
    for (const auto& [type, msg] : all) {
        const auto bytes = encode_one(msg);
        itch::CollectingHandler h;
        ASSERT_EQ(itch::parse_message(bytes, h), ParseStatus::Ok) << "type " << type;
        ASSERT_EQ(h.messages.size(), 1U) << "type " << type;
        EXPECT_EQ(h.messages[0], msg) << "type " << type;
    }
}

TEST(ParseMessage, AddOrderFromSpecBytes) {
    // Independent of the encoder: bytes laid out by hand from the spec table.
    std::vector<std::byte> b(36);
    b[0] = std::byte{'A'};
    put_be(b, 1, 0x0102, 2);
    put_be(b, 3, 0x0304, 2);
    put_be(b, 5, 0x0A0B0C0D0E0FULL, 6);
    put_be(b, 11, 0x1122334455667788ULL, 8);
    b[19] = std::byte{'S'};
    put_be(b, 20, 100, 4);
    const char* stock = "AAPL    ";
    for (std::size_t i = 0; i < 8; ++i) {
        b[24 + i] = static_cast<std::byte>(stock[i]);
    }
    put_be(b, 32, 1'500'000, 4);

    itch::CollectingHandler h;
    ASSERT_EQ(itch::parse_message(b, h), ParseStatus::Ok);
    ASSERT_EQ(h.messages.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<itch::AddOrder>(h.messages[0]));
    const itch::AddOrder expected{.header = {.stock_locate = 0x0102,
                                             .tracking_number = 0x0304,
                                             .timestamp = 0x0A0B0C0D0E0FULL},
                                  .order_reference_number = 0x1122334455667788ULL,
                                  .side = itch::Side::Sell,
                                  .shares = 100,
                                  .stock = itch::alpha<8>("AAPL"),
                                  .price = 1'500'000};
    EXPECT_EQ(h.messages[0], itch::AnyMessage{expected});
}

TEST(ParseMessage, MaxFieldValues) {
    itch::OrderExecutedWithPrice m{};
    m.header = {
        .stock_locate = 0xFFFF, .tracking_number = 0xFFFF, .timestamp = itch::max_timestamp};
    m.order_reference_number = ~0ULL;
    m.executed_shares = ~0U;
    m.match_number = ~0ULL;
    m.printable = 'Y';
    m.execution_price = ~0U;
    itch::CollectingHandler h;
    ASSERT_EQ(itch::parse_message(encode_one(m), h), ParseStatus::Ok);
    ASSERT_EQ(h.messages.size(), 1U);
    EXPECT_EQ(h.messages[0], itch::AnyMessage{m});
}

TEST(ParseMessage, EmptyIsTruncated) {
    itch::CountingHandler h;
    EXPECT_EQ(itch::parse_message({}, h), ParseStatus::Truncated);
    EXPECT_EQ(h.total, 0U);
}

TEST(ParseMessage, UnknownType) {
    for (char t : {'Z', 'a', '\0', '\xFF'}) {
        std::vector<std::byte> b(36, std::byte{0});
        b[0] = static_cast<std::byte>(t);
        itch::CountingHandler h;
        EXPECT_EQ(itch::parse_message(b, h), ParseStatus::UnknownType) << int(t);
        EXPECT_EQ(h.total, 0U);
    }
}

TEST(ParseMessage, ShortAndLongBuffersEveryType) {
    for (const auto& [type, msg] : one_of_each()) {
        auto bytes = encode_one(msg);
        itch::CountingHandler h;
        for (std::size_t n = 1; n < bytes.size(); ++n) {
            EXPECT_EQ(itch::parse_message(std::span(bytes).first(n), h), ParseStatus::Truncated)
                << "type " << type << " size " << n;
        }
        bytes.push_back(std::byte{0});
        EXPECT_EQ(itch::parse_message(bytes, h), ParseStatus::LengthMismatch) << "type " << type;
        EXPECT_EQ(h.total, 0U) << "handler must not be called on error";
    }
}

TEST(ParseMessage, InvalidSideRejected) {
    for (const char type : {'A', 'F', 'P'}) {
        auto bytes = encode_one(one_of_each().at(type));
        bytes[19] = std::byte{'X'}; // buy/sell indicator offset is 19 in A, F and P
        itch::CountingHandler h;
        EXPECT_EQ(itch::parse_message(bytes, h), ParseStatus::InvalidField) << "type " << type;
        EXPECT_EQ(h.total, 0U);
    }
}

TEST(ParseMessage, UnalignedInput) {
    // Parse from an odd address; UBSan flags misaligned loads.
    const auto bytes = encode_one(one_of_each().at('A'));
    std::vector<std::byte> buf(bytes.size() + 1);
    std::copy(bytes.begin(), bytes.end(), buf.begin() + 1);
    itch::CollectingHandler h;
    ASSERT_EQ(itch::parse_message(std::span(buf).subspan(1), h), ParseStatus::Ok);
    ASSERT_EQ(h.messages.size(), 1U);
    EXPECT_EQ(h.messages[0], one_of_each().at('A'));
}

// Property: random body bytes (valid side) parse, and re-encode to the same bytes.
RC_GTEST_PROP(ParseMessageProp, BytesRoundTripEveryType, ()) {
    static const auto types = [] {
        std::vector<char> t;
        for (const auto& [c, m] : one_of_each()) {
            t.push_back(c);
        }
        return t;
    }();
    const char type = *rc::gen::elementOf(types);
    const std::size_t n = itch::message_size(type);
    auto raw =
        *rc::gen::container<std::vector<std::uint8_t>>(n, rc::gen::arbitrary<std::uint8_t>());
    raw[0] = static_cast<std::uint8_t>(type);
    if (type == 'A' || type == 'F' || type == 'P') {
        raw[19] = *rc::gen::element<std::uint8_t>('B', 'S');
    }
    std::vector<std::byte> bytes(n);
    std::transform(raw.begin(), raw.end(), bytes.begin(),
                   [](std::uint8_t x) { return std::byte{x}; });

    itch::CollectingHandler h;
    RC_ASSERT(itch::parse_message(bytes, h) == ParseStatus::Ok);
    RC_ASSERT(h.messages.size() == 1U);
    RC_ASSERT(itch::type_of(h.messages[0]) == type);
    RC_ASSERT(encode_one(h.messages[0]) == bytes);
}

// ---- Frames ----------------------------------------------------------------------

TEST(ParseFrames, EmptyBufferIsOk) {
    itch::CountingHandler h;
    EXPECT_EQ(itch::parse_frames({}, h), (itch::FrameResult{ParseStatus::Ok, 0, 0, 0}));
}

TEST(ParseFrames, WholeFeedRoundTrips) {
    for (const auto& msgs :
         {tickpath::sim::generate_edge_cases(), tickpath::sim::generate_contract_violations(),
          tickpath::sim::generate_random({})}) {
        const auto bytes = itch::encode_framed(msgs);
        itch::CollectingHandler h;
        const auto r = itch::parse_frames(bytes, h);
        EXPECT_EQ(r, (itch::FrameResult{ParseStatus::Ok, bytes.size(), msgs.size(), 0}));
        EXPECT_EQ(h.messages, msgs);
    }
}

TEST(ParseFrames, TruncatedTailAtEveryCut) {
    const auto msgs = tickpath::sim::generate_edge_cases();
    const auto bytes = itch::encode_framed(msgs);
    // Frame start offsets.
    std::vector<std::size_t> starts;
    for (std::size_t off = 0; off < bytes.size();) {
        starts.push_back(off);
        off += 2U + ((std::to_integer<std::size_t>(bytes[off]) << 8U) |
                     std::to_integer<std::size_t>(bytes[off + 1]));
    }
    for (std::size_t cut = 1; cut < bytes.size(); ++cut) {
        const auto frame = static_cast<std::size_t>(
            std::upper_bound(starts.begin(), starts.end(), cut) - starts.begin() - 1);
        itch::CountingHandler h;
        const auto r = itch::parse_frames(std::span(bytes).first(cut), h);
        if (cut == starts[frame]) {
            EXPECT_EQ(r, (itch::FrameResult{ParseStatus::Ok, cut, frame, 0})) << "cut " << cut;
        } else {
            EXPECT_EQ(r, (itch::FrameResult{ParseStatus::Truncated, starts[frame], frame, 0}))
                << "cut " << cut;
        }
        EXPECT_EQ(h.total, frame);
    }
}

TEST(ParseFrames, ResumeFromConsumed) {
    const auto msgs = tickpath::sim::generate_edge_cases();
    const auto bytes = itch::encode_framed(msgs);
    for (std::size_t split :
         {std::size_t{1}, std::size_t{13}, bytes.size() / 2, bytes.size() - 1}) {
        itch::CollectingHandler h;
        const auto first = itch::parse_frames(std::span(bytes).first(split), h);
        const auto rest = itch::parse_frames(std::span(bytes).subspan(first.consumed), h);
        EXPECT_EQ(rest.status, ParseStatus::Ok) << "split " << split;
        EXPECT_EQ(first.consumed + rest.consumed, bytes.size());
        EXPECT_EQ(h.messages, msgs) << "split " << split;
    }
}

TEST(ParseFrames, UnknownTypeIsSkipped) {
    const std::vector<itch::AnyMessage> msgs{
        itch::SystemEvent{.header = {}, .event_code = 'O'},
        itch::OrderDelete{.header = {}, .order_reference_number = 5}};
    auto bytes = itch::encode_framed(std::span(msgs).first(1));
    const std::vector<std::byte> unknown{std::byte{0}, std::byte{5}, std::byte{'Z'}, std::byte{1},
                                         std::byte{2}, std::byte{3}, std::byte{4}};
    bytes.insert(bytes.end(), unknown.begin(), unknown.end());
    const auto tail = itch::encode_framed(std::span(msgs).subspan(1));
    bytes.insert(bytes.end(), tail.begin(), tail.end());

    itch::CollectingHandler h;
    EXPECT_EQ(itch::parse_frames(bytes, h),
              (itch::FrameResult{ParseStatus::Ok, bytes.size(), 2, 1}));
    EXPECT_EQ(h.messages, msgs);
}

TEST(ParseFrames, ZeroLengthFrame) {
    const std::vector<std::byte> bytes{std::byte{0}, std::byte{0}, std::byte{'S'}};
    itch::CountingHandler h;
    EXPECT_EQ(itch::parse_frames(bytes, h),
              (itch::FrameResult{ParseStatus::LengthMismatch, 0, 0, 0}));
}

TEST(ParseFrames, KnownTypeWrongFrameLength) {
    const std::vector<itch::AnyMessage> msgs{itch::SystemEvent{}, itch::AddOrder{}};
    auto bytes = itch::encode_framed(msgs);
    // Second frame claims 37 bytes for a 36-byte Add Order; give it 37.
    const std::size_t second = 2 + 12;
    bytes[second + 1] = std::byte{37};
    bytes.push_back(std::byte{0});
    itch::CountingHandler h;
    EXPECT_EQ(itch::parse_frames(bytes, h),
              (itch::FrameResult{ParseStatus::LengthMismatch, second, 1, 0}));
    EXPECT_EQ(h.total, 1U);
}

TEST(ParseFrames, InvalidFieldStopsAtFrame) {
    const std::vector<itch::AnyMessage> msgs{itch::SystemEvent{}, itch::AddOrder{},
                                             itch::SystemEvent{}};
    auto bytes = itch::encode_framed(msgs);
    const std::size_t second = 2 + 12;
    bytes[second + 2 + 19] = std::byte{'?'};
    itch::CountingHandler h;
    EXPECT_EQ(itch::parse_frames(bytes, h),
              (itch::FrameResult{ParseStatus::InvalidField, second, 1, 0}));
}
