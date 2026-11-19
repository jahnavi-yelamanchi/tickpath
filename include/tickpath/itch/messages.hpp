#pragma once

// Nasdaq TotalView-ITCH 5.0 message definitions.
//
// Each struct holds the decoded (host-order) fields of one message type and
// carries the wire layout from the specification: the type character, the
// total message length, and the byte offset of every field. Offsets count
// from the message type byte, as in the spec tables. The static_asserts below
// check that the last field ends exactly at the message length.
//
// Spec version and verification status: docs/VERSIONS.md.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <variant>

#include "tickpath/itch/types.hpp"

namespace tickpath::itch {

/// Offsets shared by every message.
struct HeaderOffset {
    static constexpr std::size_t message_type = 0;    // Alpha(1)
    static constexpr std::size_t stock_locate = 1;    // Integer(2)
    static constexpr std::size_t tracking_number = 3; // Integer(2)
    static constexpr std::size_t timestamp = 5;       // Integer(6)
    static constexpr std::size_t body = 11;
};

// 1.1 System Event
struct SystemEvent {
    static constexpr char type = 'S';
    static constexpr std::size_t size = 12;
    struct Offset {
        static constexpr std::size_t event_code = 11; // Alpha(1)
    };
    Header header;
    char event_code{}; // 'O','S','Q','M','E','C'
    friend bool operator==(const SystemEvent&, const SystemEvent&) = default;
};

// 1.2.1 Stock Directory
struct StockDirectory {
    static constexpr char type = 'R';
    static constexpr std::size_t size = 39;
    struct Offset {
        static constexpr std::size_t stock = 11;                          // Alpha(8)
        static constexpr std::size_t market_category = 19;                // Alpha(1)
        static constexpr std::size_t financial_status_indicator = 20;     // Alpha(1)
        static constexpr std::size_t round_lot_size = 21;                 // Integer(4)
        static constexpr std::size_t round_lots_only = 25;                // Alpha(1)
        static constexpr std::size_t issue_classification = 26;           // Alpha(1)
        static constexpr std::size_t issue_sub_type = 27;                 // Alpha(2)
        static constexpr std::size_t authenticity = 29;                   // Alpha(1)
        static constexpr std::size_t short_sale_threshold_indicator = 30; // Alpha(1)
        static constexpr std::size_t ipo_flag = 31;                       // Alpha(1)
        static constexpr std::size_t luld_reference_price_tier = 32;      // Alpha(1)
        static constexpr std::size_t etp_flag = 33;                       // Alpha(1)
        static constexpr std::size_t etp_leverage_factor = 34;            // Integer(4)
        static constexpr std::size_t inverse_indicator = 38;              // Alpha(1)
    };
    Header header;
    Stock stock{};
    char market_category{};
    char financial_status_indicator{};
    std::uint32_t round_lot_size{};
    char round_lots_only{};
    char issue_classification{};
    std::array<char, 2> issue_sub_type{};
    char authenticity{};
    char short_sale_threshold_indicator{};
    char ipo_flag{};
    char luld_reference_price_tier{};
    char etp_flag{};
    std::uint32_t etp_leverage_factor{};
    char inverse_indicator{};
    friend bool operator==(const StockDirectory&, const StockDirectory&) = default;
};

// 1.2.2 Stock Trading Action
struct StockTradingAction {
    static constexpr char type = 'H';
    static constexpr std::size_t size = 25;
    struct Offset {
        static constexpr std::size_t stock = 11;         // Alpha(8)
        static constexpr std::size_t trading_state = 19; // Alpha(1)
        static constexpr std::size_t reserved = 20;      // Alpha(1)
        static constexpr std::size_t reason = 21;        // Alpha(4)
    };
    Header header;
    Stock stock{};
    char trading_state{};
    char reserved{};
    std::array<char, 4> reason{};
    friend bool operator==(const StockTradingAction&, const StockTradingAction&) = default;
};

// 1.2.3 Reg SHO Short Sale Price Test Restricted Indicator
struct RegShoRestriction {
    static constexpr char type = 'Y';
    static constexpr std::size_t size = 20;
    struct Offset {
        static constexpr std::size_t stock = 11;          // Alpha(8)
        static constexpr std::size_t reg_sho_action = 19; // Alpha(1)
    };
    Header header;
    Stock stock{};
    char reg_sho_action{};
    friend bool operator==(const RegShoRestriction&, const RegShoRestriction&) = default;
};

// 1.2.4 Market Participant Position
struct MarketParticipantPosition {
    static constexpr char type = 'L';
    static constexpr std::size_t size = 26;
    struct Offset {
        static constexpr std::size_t mpid = 11;                     // Alpha(4)
        static constexpr std::size_t stock = 15;                    // Alpha(8)
        static constexpr std::size_t primary_market_maker = 23;     // Alpha(1)
        static constexpr std::size_t market_maker_mode = 24;        // Alpha(1)
        static constexpr std::size_t market_participant_state = 25; // Alpha(1)
    };
    Header header;
    Mpid mpid{};
    Stock stock{};
    char primary_market_maker{};
    char market_maker_mode{};
    char market_participant_state{};
    friend bool operator==(const MarketParticipantPosition&,
                           const MarketParticipantPosition&) = default;
};

// 1.2.5.1 MWCB Decline Level
struct MwcbDeclineLevel {
    static constexpr char type = 'V';
    static constexpr std::size_t size = 35;
    struct Offset {
        static constexpr std::size_t level1 = 11; // Price(8)
        static constexpr std::size_t level2 = 19; // Price(8)
        static constexpr std::size_t level3 = 27; // Price(8)
    };
    Header header;
    Price8 level1{};
    Price8 level2{};
    Price8 level3{};
    friend bool operator==(const MwcbDeclineLevel&, const MwcbDeclineLevel&) = default;
};

// 1.2.5.2 MWCB Status
struct MwcbStatus {
    static constexpr char type = 'W';
    static constexpr std::size_t size = 12;
    struct Offset {
        static constexpr std::size_t breached_level = 11; // Alpha(1)
    };
    Header header;
    char breached_level{};
    friend bool operator==(const MwcbStatus&, const MwcbStatus&) = default;
};

// 1.2.6 IPO Quoting Period Update
struct IpoQuotingPeriodUpdate {
    static constexpr char type = 'K';
    static constexpr std::size_t size = 28;
    struct Offset {
        static constexpr std::size_t stock = 11; // Alpha(8)
        static constexpr std::size_t ipo_quotation_release_time =
            19; // Integer(4), seconds since midnight
        static constexpr std::size_t ipo_quotation_release_qualifier = 23; // Alpha(1)
        static constexpr std::size_t ipo_price = 24;                       // Price(4)
    };
    Header header;
    Stock stock{};
    std::uint32_t ipo_quotation_release_time{};
    char ipo_quotation_release_qualifier{};
    Price4 ipo_price{};
    friend bool operator==(const IpoQuotingPeriodUpdate&, const IpoQuotingPeriodUpdate&) = default;
};

// 1.2.7 Limit Up - Limit Down (LULD) Auction Collar
struct LuldAuctionCollar {
    static constexpr char type = 'J';
    static constexpr std::size_t size = 35;
    struct Offset {
        static constexpr std::size_t stock = 11;                          // Alpha(8)
        static constexpr std::size_t auction_collar_reference_price = 19; // Price(4)
        static constexpr std::size_t upper_auction_collar_price = 23;     // Price(4)
        static constexpr std::size_t lower_auction_collar_price = 27;     // Price(4)
        static constexpr std::size_t auction_collar_extension = 31;       // Integer(4)
    };
    Header header;
    Stock stock{};
    Price4 auction_collar_reference_price{};
    Price4 upper_auction_collar_price{};
    Price4 lower_auction_collar_price{};
    std::uint32_t auction_collar_extension{};
    friend bool operator==(const LuldAuctionCollar&, const LuldAuctionCollar&) = default;
};

// 1.2.8 Operational Halt
struct OperationalHalt {
    static constexpr char type = 'h';
    static constexpr std::size_t size = 21;
    struct Offset {
        static constexpr std::size_t stock = 11;                   // Alpha(8)
        static constexpr std::size_t market_code = 19;             // Alpha(1)
        static constexpr std::size_t operational_halt_action = 20; // Alpha(1)
    };
    Header header;
    Stock stock{};
    char market_code{};
    char operational_halt_action{};
    friend bool operator==(const OperationalHalt&, const OperationalHalt&) = default;
};

// 1.3.1 Add Order - No MPID Attribution
struct AddOrder {
    static constexpr char type = 'A';
    static constexpr std::size_t size = 36;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
        static constexpr std::size_t buy_sell_indicator = 19;     // Alpha(1)
        static constexpr std::size_t shares = 20;                 // Integer(4)
        static constexpr std::size_t stock = 24;                  // Alpha(8)
        static constexpr std::size_t price = 32;                  // Price(4)
    };
    Header header;
    OrderRef order_reference_number{};
    Side side{Side::Buy};
    Shares shares{};
    Stock stock{};
    Price4 price{};
    friend bool operator==(const AddOrder&, const AddOrder&) = default;
};

// 1.3.2 Add Order with MPID Attribution
struct AddOrderMpid {
    static constexpr char type = 'F';
    static constexpr std::size_t size = 40;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
        static constexpr std::size_t buy_sell_indicator = 19;     // Alpha(1)
        static constexpr std::size_t shares = 20;                 // Integer(4)
        static constexpr std::size_t stock = 24;                  // Alpha(8)
        static constexpr std::size_t price = 32;                  // Price(4)
        static constexpr std::size_t attribution = 36;            // Alpha(4)
    };
    Header header;
    OrderRef order_reference_number{};
    Side side{Side::Buy};
    Shares shares{};
    Stock stock{};
    Price4 price{};
    Mpid attribution{};
    friend bool operator==(const AddOrderMpid&, const AddOrderMpid&) = default;
};

// 1.4.1 Order Executed
struct OrderExecuted {
    static constexpr char type = 'E';
    static constexpr std::size_t size = 31;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
        static constexpr std::size_t executed_shares = 19;        // Integer(4)
        static constexpr std::size_t match_number = 23;           // Integer(8)
    };
    Header header;
    OrderRef order_reference_number{};
    Shares executed_shares{};
    MatchNumber match_number{};
    friend bool operator==(const OrderExecuted&, const OrderExecuted&) = default;
};

// 1.4.2 Order Executed With Price
struct OrderExecutedWithPrice {
    static constexpr char type = 'C';
    static constexpr std::size_t size = 36;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
        static constexpr std::size_t executed_shares = 19;        // Integer(4)
        static constexpr std::size_t match_number = 23;           // Integer(8)
        static constexpr std::size_t printable = 31;              // Alpha(1)
        static constexpr std::size_t execution_price = 32;        // Price(4)
    };
    Header header;
    OrderRef order_reference_number{};
    Shares executed_shares{};
    MatchNumber match_number{};
    char printable{};
    Price4 execution_price{};
    friend bool operator==(const OrderExecutedWithPrice&, const OrderExecutedWithPrice&) = default;
};

// 1.4.3 Order Cancel
struct OrderCancel {
    static constexpr char type = 'X';
    static constexpr std::size_t size = 23;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
        static constexpr std::size_t cancelled_shares = 19;       // Integer(4)
    };
    Header header;
    OrderRef order_reference_number{};
    Shares cancelled_shares{};
    friend bool operator==(const OrderCancel&, const OrderCancel&) = default;
};

// 1.4.4 Order Delete
struct OrderDelete {
    static constexpr char type = 'D';
    static constexpr std::size_t size = 19;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
    };
    Header header;
    OrderRef order_reference_number{};
    friend bool operator==(const OrderDelete&, const OrderDelete&) = default;
};

// 1.4.5 Order Replace
struct OrderReplace {
    static constexpr char type = 'U';
    static constexpr std::size_t size = 35;
    struct Offset {
        static constexpr std::size_t original_order_reference_number = 11; // Integer(8)
        static constexpr std::size_t new_order_reference_number = 19;      // Integer(8)
        static constexpr std::size_t shares = 27;                          // Integer(4)
        static constexpr std::size_t price = 31;                           // Price(4)
    };
    Header header;
    OrderRef original_order_reference_number{};
    OrderRef new_order_reference_number{};
    Shares shares{};
    Price4 price{};
    friend bool operator==(const OrderReplace&, const OrderReplace&) = default;
};

// 1.5.1 Trade Message (Non-Cross)
struct Trade {
    static constexpr char type = 'P';
    static constexpr std::size_t size = 44;
    struct Offset {
        static constexpr std::size_t order_reference_number = 11; // Integer(8)
        static constexpr std::size_t buy_sell_indicator = 19;     // Alpha(1)
        static constexpr std::size_t shares = 20;                 // Integer(4)
        static constexpr std::size_t stock = 24;                  // Alpha(8)
        static constexpr std::size_t price = 32;                  // Price(4)
        static constexpr std::size_t match_number = 36;           // Integer(8)
    };
    Header header;
    OrderRef order_reference_number{};
    Side side{Side::Buy};
    Shares shares{};
    Stock stock{};
    Price4 price{};
    MatchNumber match_number{};
    friend bool operator==(const Trade&, const Trade&) = default;
};

// 1.5.2 Cross Trade
struct CrossTrade {
    static constexpr char type = 'Q';
    static constexpr std::size_t size = 40;
    struct Offset {
        static constexpr std::size_t shares = 11;       // Integer(8)
        static constexpr std::size_t stock = 19;        // Alpha(8)
        static constexpr std::size_t cross_price = 27;  // Price(4)
        static constexpr std::size_t match_number = 31; // Integer(8)
        static constexpr std::size_t cross_type = 39;   // Alpha(1)
    };
    Header header;
    std::uint64_t shares{};
    Stock stock{};
    Price4 cross_price{};
    MatchNumber match_number{};
    char cross_type{};
    friend bool operator==(const CrossTrade&, const CrossTrade&) = default;
};

// 1.5.3 Broken Trade / Order Execution
struct BrokenTrade {
    static constexpr char type = 'B';
    static constexpr std::size_t size = 19;
    struct Offset {
        static constexpr std::size_t match_number = 11; // Integer(8)
    };
    Header header;
    MatchNumber match_number{};
    friend bool operator==(const BrokenTrade&, const BrokenTrade&) = default;
};

// 1.6 Net Order Imbalance Indicator (NOII)
struct Noii {
    static constexpr char type = 'I';
    static constexpr std::size_t size = 50;
    struct Offset {
        static constexpr std::size_t paired_shares = 11;             // Integer(8)
        static constexpr std::size_t imbalance_shares = 19;          // Integer(8)
        static constexpr std::size_t imbalance_direction = 27;       // Alpha(1)
        static constexpr std::size_t stock = 28;                     // Alpha(8)
        static constexpr std::size_t far_price = 36;                 // Price(4)
        static constexpr std::size_t near_price = 40;                // Price(4)
        static constexpr std::size_t current_reference_price = 44;   // Price(4)
        static constexpr std::size_t cross_type = 48;                // Alpha(1)
        static constexpr std::size_t price_variation_indicator = 49; // Alpha(1)
    };
    Header header;
    std::uint64_t paired_shares{};
    std::uint64_t imbalance_shares{};
    char imbalance_direction{};
    Stock stock{};
    Price4 far_price{};
    Price4 near_price{};
    Price4 current_reference_price{};
    char cross_type{};
    char price_variation_indicator{};
    friend bool operator==(const Noii&, const Noii&) = default;
};

// 1.7 Retail Price Improvement Indicator (RPII)
struct RetailPriceImprovement {
    static constexpr char type = 'N';
    static constexpr std::size_t size = 20;
    struct Offset {
        static constexpr std::size_t stock = 11;         // Alpha(8)
        static constexpr std::size_t interest_flag = 19; // Alpha(1)
    };
    Header header;
    Stock stock{};
    char interest_flag{};
    friend bool operator==(const RetailPriceImprovement&, const RetailPriceImprovement&) = default;
};

// 1.8 Direct Listing with Capital Raise Price Discovery
struct DlcrPriceDiscovery {
    static constexpr char type = 'O';
    static constexpr std::size_t size = 48;
    struct Offset {
        static constexpr std::size_t stock = 11;                    // Alpha(8)
        static constexpr std::size_t open_eligibility_status = 19;  // Alpha(1)
        static constexpr std::size_t minimum_allowable_price = 20;  // Price(4)
        static constexpr std::size_t maximum_allowable_price = 24;  // Price(4)
        static constexpr std::size_t near_execution_price = 28;     // Price(4)
        static constexpr std::size_t near_execution_time = 32;      // Integer(8), ns since midnight
        static constexpr std::size_t lower_price_range_collar = 40; // Price(4)
        static constexpr std::size_t upper_price_range_collar = 44; // Price(4)
    };
    Header header;
    Stock stock{};
    char open_eligibility_status{};
    Price4 minimum_allowable_price{};
    Price4 maximum_allowable_price{};
    Price4 near_execution_price{};
    std::uint64_t near_execution_time{};
    Price4 lower_price_range_collar{};
    Price4 upper_price_range_collar{};
    friend bool operator==(const DlcrPriceDiscovery&, const DlcrPriceDiscovery&) = default;
};

// ---- Layout checks: last field offset + width == total length --------------

static_assert(SystemEvent::Offset::event_code + 1 == SystemEvent::size);
static_assert(StockDirectory::Offset::inverse_indicator + 1 == StockDirectory::size);
static_assert(StockTradingAction::Offset::reason + 4 == StockTradingAction::size);
static_assert(RegShoRestriction::Offset::reg_sho_action + 1 == RegShoRestriction::size);
static_assert(MarketParticipantPosition::Offset::market_participant_state + 1 ==
              MarketParticipantPosition::size);
static_assert(MwcbDeclineLevel::Offset::level3 + 8 == MwcbDeclineLevel::size);
static_assert(MwcbStatus::Offset::breached_level + 1 == MwcbStatus::size);
static_assert(IpoQuotingPeriodUpdate::Offset::ipo_price + 4 == IpoQuotingPeriodUpdate::size);
static_assert(LuldAuctionCollar::Offset::auction_collar_extension + 4 == LuldAuctionCollar::size);
static_assert(OperationalHalt::Offset::operational_halt_action + 1 == OperationalHalt::size);
static_assert(AddOrder::Offset::price + 4 == AddOrder::size);
static_assert(AddOrderMpid::Offset::attribution + 4 == AddOrderMpid::size);
static_assert(OrderExecuted::Offset::match_number + 8 == OrderExecuted::size);
static_assert(OrderExecutedWithPrice::Offset::execution_price + 4 == OrderExecutedWithPrice::size);
static_assert(OrderCancel::Offset::cancelled_shares + 4 == OrderCancel::size);
static_assert(OrderDelete::Offset::order_reference_number + 8 == OrderDelete::size);
static_assert(OrderReplace::Offset::price + 4 == OrderReplace::size);
static_assert(Trade::Offset::match_number + 8 == Trade::size);
static_assert(CrossTrade::Offset::cross_type + 1 == CrossTrade::size);
static_assert(BrokenTrade::Offset::match_number + 8 == BrokenTrade::size);
static_assert(Noii::Offset::price_variation_indicator + 1 == Noii::size);
static_assert(RetailPriceImprovement::Offset::interest_flag + 1 == RetailPriceImprovement::size);
static_assert(DlcrPriceDiscovery::Offset::upper_price_range_collar + 4 == DlcrPriceDiscovery::size);

// ---- Type list -------------------------------------------------------------

/// Every ITCH 5.0 message, decoded. Used by the generator, encoder, tests and
/// oracle. Not used on the hot path: the parser calls a typed handler instead.
using AnyMessage =
    std::variant<SystemEvent, StockDirectory, StockTradingAction, RegShoRestriction,
                 MarketParticipantPosition, MwcbDeclineLevel, MwcbStatus, IpoQuotingPeriodUpdate,
                 LuldAuctionCollar, OperationalHalt, AddOrder, AddOrderMpid, OrderExecuted,
                 OrderExecutedWithPrice, OrderCancel, OrderDelete, OrderReplace, Trade, CrossTrade,
                 BrokenTrade, Noii, RetailPriceImprovement, DlcrPriceDiscovery>;

inline constexpr std::size_t message_type_count = std::variant_size_v<AnyMessage>;
static_assert(message_type_count == 23);

/// Longest message (NOII).
inline constexpr std::size_t max_message_size = Noii::size;

namespace detail {
template <class... Ts>
constexpr std::array<std::uint8_t, 256> make_size_table(std::variant<Ts...>* /*tag*/) noexcept {
    std::array<std::uint8_t, 256> t{};
    ((t[static_cast<unsigned char>(Ts::type)] = static_cast<std::uint8_t>(Ts::size)), ...);
    return t;
}
} // namespace detail

/// Wire length indexed by message type byte; 0 for an unknown type.
inline constexpr std::array<std::uint8_t, 256> message_size_table =
    detail::make_size_table(static_cast<AnyMessage*>(nullptr));

/// Wire length for a message type byte; 0 for an unknown type.
[[nodiscard]] constexpr std::size_t message_size(char type) noexcept {
    return message_size_table[static_cast<unsigned char>(type)];
}

/// Header of any decoded message.
[[nodiscard]] inline const Header& header_of(const AnyMessage& m) {
    return std::visit([](const auto& x) -> const Header& { return x.header; }, m);
}

/// Type character of any decoded message.
[[nodiscard]] inline char type_of(const AnyMessage& m) {
    return std::visit([](const auto& x) { return std::decay_t<decltype(x)>::type; }, m);
}

} // namespace tickpath::itch
