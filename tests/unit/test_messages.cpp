// Message definitions and encoder. The encoder is the oracle for the parser,
// so it is checked against hand-written bytes taken from the spec layout.

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "tickpath/itch/encode.hpp"
#include "tickpath/itch/messages.hpp"

namespace itch = tickpath::itch;

namespace {

std::vector<std::byte> bytes(std::initializer_list<int> v) {
    std::vector<std::byte> out;
    out.reserve(v.size());
    for (int x : v) {
        out.push_back(static_cast<std::byte>(x));
    }
    return out;
}

std::vector<std::byte> encode_one(const itch::AnyMessage& m) {
    std::vector<std::byte> out(itch::max_message_size);
    out.resize(itch::encode(m, out));
    return out;
}

constexpr itch::Header golden_header{
    .stock_locate = 0x0102, .tracking_number = 0x0304, .timestamp = 0x0A0B0C0D0E0FULL};

} // namespace

TEST(MessageTable, EveryTypeHasDistinctTypeAndSpecLength) {
    // Lengths from the ITCH 5.0 spec tables.
    const std::array<std::pair<char, std::size_t>, 23> expected{{
        {'S', 12}, {'R', 39}, {'H', 25}, {'Y', 20}, {'L', 26}, {'V', 35}, {'W', 12}, {'K', 28},
        {'J', 35}, {'h', 21}, {'A', 36}, {'F', 40}, {'E', 31}, {'C', 36}, {'X', 23}, {'D', 19},
        {'U', 35}, {'P', 44}, {'Q', 40}, {'B', 19}, {'I', 50}, {'N', 20}, {'O', 48},
    }};
    std::set<char> seen;
    for (const auto& [type, size] : expected) {
        EXPECT_EQ(itch::message_size(type), size) << "type " << type;
        EXPECT_TRUE(seen.insert(type).second);
    }
    std::size_t known = 0;
    for (int c = 0; c < 256; ++c) {
        if (itch::message_size(static_cast<char>(c)) != 0) {
            ++known;
        }
    }
    EXPECT_EQ(known, itch::message_type_count);
}

TEST(MessageTable, UnknownTypesHaveZeroLength) {
    for (char c : {'\0', 'Z', 'a', 'z', '1', ' '}) {
        EXPECT_EQ(itch::message_size(c), 0U) << "type " << c;
    }
}

TEST(Encoder, AddOrderGoldenBytes) {
    const itch::AddOrder m{.header = golden_header,
                           .order_reference_number = 0x1122334455667788ULL,
                           .side = itch::Side::Sell,
                           .shares = 100,
                           .stock = itch::alpha<8>("AAPL"),
                           .price = 1'500'000}; // 150.0000
    const auto expected = bytes(
        {'A',  0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,         // header
         0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,                           // order ref
         'S',  0x00, 0x00, 0x00, 0x64,                                             // side, shares
         'A',  'A',  'P',  'L',  ' ',  ' ',  ' ',  ' ',  0x00, 0x16, 0xE3, 0x60}); // stock, price
    EXPECT_EQ(encode_one(m), expected);
}

TEST(Encoder, OrderExecutedWithPriceGoldenBytes) {
    const itch::OrderExecutedWithPrice m{.header = golden_header,
                                         .order_reference_number = 7,
                                         .executed_shares = 0x01020304,
                                         .match_number = 0xFFEEDDCCBBAA9988ULL,
                                         .printable = 'N',
                                         .execution_price = 0x7FFFFFFF};
    const auto expected =
        bytes({'C',  0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0,
               0,    0,    0,    0,    0,    0,    7,    0x01, 0x02, 0x03, 0x04, 0xFF,
               0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 'N',  0x7F, 0xFF, 0xFF, 0xFF});
    EXPECT_EQ(encode_one(m), expected);
}

TEST(Encoder, OrderReplaceGoldenBytes) {
    const itch::OrderReplace m{.header = golden_header,
                               .original_order_reference_number = 1,
                               .new_order_reference_number = 2,
                               .shares = 300,
                               .price = 10'000};
    const auto expected = bytes({'U', 0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0,
                                 0,   0,    0,    0,    0,    0,    1,    0,    0,    0,    0,    0,
                                 0,   0,    2,    0x00, 0x00, 0x01, 0x2C, 0x00, 0x00, 0x27, 0x10});
    EXPECT_EQ(encode_one(m), expected);
}

TEST(Encoder, MwcbDeclineLevelUsesPrice8) {
    const itch::MwcbDeclineLevel m{
        .header = golden_header, .level1 = 0x0102030405060708ULL, .level2 = 0, .level3 = ~0ULL};
    const auto out = encode_one(m);
    ASSERT_EQ(out.size(), 35U);
    EXPECT_EQ(out[11], std::byte{0x01});
    EXPECT_EQ(out[18], std::byte{0x08});
    EXPECT_EQ(out[27], std::byte{0xFF});
    EXPECT_EQ(out[34], std::byte{0xFF});
}

TEST(Encoder, StockDirectoryFieldPositions) {
    itch::StockDirectory m{};
    m.header = golden_header;
    m.stock = itch::alpha<8>("MSFT");
    m.market_category = 'Q';
    m.financial_status_indicator = 'N';
    m.round_lot_size = 100;
    m.round_lots_only = 'N';
    m.issue_classification = 'C';
    m.issue_sub_type = itch::alpha<2>("Z");
    m.authenticity = 'P';
    m.short_sale_threshold_indicator = 'N';
    m.ipo_flag = ' ';
    m.luld_reference_price_tier = '1';
    m.etp_flag = 'N';
    m.etp_leverage_factor = 0;
    m.inverse_indicator = 'N';
    const auto out = encode_one(m);
    ASSERT_EQ(out.size(), 39U);
    EXPECT_EQ(out[19], std::byte{'Q'});
    EXPECT_EQ(out[24], std::byte{100});
    EXPECT_EQ(out[27], std::byte{'Z'});
    EXPECT_EQ(out[28], std::byte{' '});
    EXPECT_EQ(out[32], std::byte{'1'});
    EXPECT_EQ(out[38], std::byte{'N'});
}

TEST(Encoder, EveryTypeWritesTypeByteHeaderAndExactLength) {
    const std::vector<itch::AnyMessage> all = {itch::SystemEvent{},
                                               itch::StockDirectory{},
                                               itch::StockTradingAction{},
                                               itch::RegShoRestriction{},
                                               itch::MarketParticipantPosition{},
                                               itch::MwcbDeclineLevel{},
                                               itch::MwcbStatus{},
                                               itch::IpoQuotingPeriodUpdate{},
                                               itch::LuldAuctionCollar{},
                                               itch::OperationalHalt{},
                                               itch::AddOrder{},
                                               itch::AddOrderMpid{},
                                               itch::OrderExecuted{},
                                               itch::OrderExecutedWithPrice{},
                                               itch::OrderCancel{},
                                               itch::OrderDelete{},
                                               itch::OrderReplace{},
                                               itch::Trade{},
                                               itch::CrossTrade{},
                                               itch::BrokenTrade{},
                                               itch::Noii{},
                                               itch::RetailPriceImprovement{},
                                               itch::DlcrPriceDiscovery{}};
    ASSERT_EQ(all.size(), itch::message_type_count);
    for (auto m : all) {
        std::visit([](auto& x) { x.header = golden_header; }, m);
        const auto out = encode_one(m);
        const char type = itch::type_of(m);
        EXPECT_EQ(out.size(), itch::message_size(type)) << type;
        EXPECT_EQ(out[0], static_cast<std::byte>(type));
        const auto hdr = bytes({0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F});
        EXPECT_TRUE(std::equal(hdr.begin(), hdr.end(), out.begin() + 1)) << type;
    }
}

TEST(Encoder, FramingIsTwoByteBigEndianLengthPrefix) {
    const std::vector<itch::AnyMessage> msgs = {itch::SystemEvent{.header = {}, .event_code = 'O'},
                                                itch::Noii{}};
    const auto out = itch::encode_framed(msgs);
    ASSERT_EQ(out.size(), 2U + 12U + 2U + 50U);
    EXPECT_EQ(out[0], std::byte{0});
    EXPECT_EQ(out[1], std::byte{12});
    EXPECT_EQ(out[2], std::byte{'S'});
    EXPECT_EQ(out[13], std::byte{'O'});
    EXPECT_EQ(out[14], std::byte{0});
    EXPECT_EQ(out[15], std::byte{50});
    EXPECT_EQ(out[16], std::byte{'I'});
}

TEST(Encoder, TimestampBoundaries) {
    itch::SystemEvent m{};
    m.header.timestamp = itch::max_timestamp;
    const auto out = encode_one(m);
    for (std::size_t i = 5; i < 11; ++i) {
        EXPECT_EQ(out[i], std::byte{0xFF});
    }
    m.header.timestamp = itch::max_timestamp + 1;
    std::vector<std::byte> buf(64);
    EXPECT_THROW((void)itch::encode(m, buf), std::out_of_range);
}

TEST(Encoder, RejectsShortBuffer) {
    std::vector<std::byte> buf(35);
    EXPECT_THROW((void)itch::encode(itch::AddOrder{}, buf), std::length_error);
}
