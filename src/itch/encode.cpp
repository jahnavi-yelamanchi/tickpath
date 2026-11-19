#include "tickpath/itch/encode.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace tickpath::itch {
namespace {

class Writer {
public:
    explicit Writer(std::span<std::byte> out) noexcept
        : out_(out) {}

    void u8_at(std::size_t off, std::uint8_t v) noexcept { out_[off] = std::byte{v}; }
    void ch_at(std::size_t off, char c) noexcept { u8_at(off, static_cast<std::uint8_t>(c)); }

    template <std::size_t N>
    void be_at(std::size_t off, std::uint64_t v) noexcept {
        static_assert(N >= 1 && N <= 8);
        for (std::size_t i = 0; i < N; ++i) {
            const auto shift = 8U * (N - 1U - i);
            out_[off + i] = std::byte{static_cast<std::uint8_t>((v >> shift) & 0xFFU)};
        }
    }

    template <std::size_t N>
    void alpha_at(std::size_t off, const std::array<char, N>& a) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            ch_at(off + i, a[i]);
        }
    }

    void header(char type, const Header& h) {
        if (h.timestamp > max_timestamp) {
            throw std::out_of_range("ITCH timestamp exceeds 48 bits");
        }
        ch_at(HeaderOffset::message_type, type);
        be_at<2>(HeaderOffset::stock_locate, h.stock_locate);
        be_at<2>(HeaderOffset::tracking_number, h.tracking_number);
        be_at<6>(HeaderOffset::timestamp, h.timestamp);
    }

private:
    std::span<std::byte> out_;
};

template <class M>
void write_body(Writer& w, const M& m);

template <>
void write_body(Writer& w, const SystemEvent& m) {
    w.ch_at(SystemEvent::Offset::event_code, m.event_code);
}

template <>
void write_body(Writer& w, const StockDirectory& m) {
    using O = StockDirectory::Offset;
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::market_category, m.market_category);
    w.ch_at(O::financial_status_indicator, m.financial_status_indicator);
    w.be_at<4>(O::round_lot_size, m.round_lot_size);
    w.ch_at(O::round_lots_only, m.round_lots_only);
    w.ch_at(O::issue_classification, m.issue_classification);
    w.alpha_at(O::issue_sub_type, m.issue_sub_type);
    w.ch_at(O::authenticity, m.authenticity);
    w.ch_at(O::short_sale_threshold_indicator, m.short_sale_threshold_indicator);
    w.ch_at(O::ipo_flag, m.ipo_flag);
    w.ch_at(O::luld_reference_price_tier, m.luld_reference_price_tier);
    w.ch_at(O::etp_flag, m.etp_flag);
    w.be_at<4>(O::etp_leverage_factor, m.etp_leverage_factor);
    w.ch_at(O::inverse_indicator, m.inverse_indicator);
}

template <>
void write_body(Writer& w, const StockTradingAction& m) {
    using O = StockTradingAction::Offset;
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::trading_state, m.trading_state);
    w.ch_at(O::reserved, m.reserved);
    w.alpha_at(O::reason, m.reason);
}

template <>
void write_body(Writer& w, const RegShoRestriction& m) {
    using O = RegShoRestriction::Offset;
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::reg_sho_action, m.reg_sho_action);
}

template <>
void write_body(Writer& w, const MarketParticipantPosition& m) {
    using O = MarketParticipantPosition::Offset;
    w.alpha_at(O::mpid, m.mpid);
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::primary_market_maker, m.primary_market_maker);
    w.ch_at(O::market_maker_mode, m.market_maker_mode);
    w.ch_at(O::market_participant_state, m.market_participant_state);
}

template <>
void write_body(Writer& w, const MwcbDeclineLevel& m) {
    using O = MwcbDeclineLevel::Offset;
    w.be_at<8>(O::level1, m.level1);
    w.be_at<8>(O::level2, m.level2);
    w.be_at<8>(O::level3, m.level3);
}

template <>
void write_body(Writer& w, const MwcbStatus& m) {
    w.ch_at(MwcbStatus::Offset::breached_level, m.breached_level);
}

template <>
void write_body(Writer& w, const IpoQuotingPeriodUpdate& m) {
    using O = IpoQuotingPeriodUpdate::Offset;
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::ipo_quotation_release_time, m.ipo_quotation_release_time);
    w.ch_at(O::ipo_quotation_release_qualifier, m.ipo_quotation_release_qualifier);
    w.be_at<4>(O::ipo_price, m.ipo_price);
}

template <>
void write_body(Writer& w, const LuldAuctionCollar& m) {
    using O = LuldAuctionCollar::Offset;
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::auction_collar_reference_price, m.auction_collar_reference_price);
    w.be_at<4>(O::upper_auction_collar_price, m.upper_auction_collar_price);
    w.be_at<4>(O::lower_auction_collar_price, m.lower_auction_collar_price);
    w.be_at<4>(O::auction_collar_extension, m.auction_collar_extension);
}

template <>
void write_body(Writer& w, const OperationalHalt& m) {
    using O = OperationalHalt::Offset;
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::market_code, m.market_code);
    w.ch_at(O::operational_halt_action, m.operational_halt_action);
}

template <>
void write_body(Writer& w, const AddOrder& m) {
    using O = AddOrder::Offset;
    w.be_at<8>(O::order_reference_number, m.order_reference_number);
    w.ch_at(O::buy_sell_indicator, static_cast<char>(m.side));
    w.be_at<4>(O::shares, m.shares);
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::price, m.price);
}

template <>
void write_body(Writer& w, const AddOrderMpid& m) {
    using O = AddOrderMpid::Offset;
    w.be_at<8>(O::order_reference_number, m.order_reference_number);
    w.ch_at(O::buy_sell_indicator, static_cast<char>(m.side));
    w.be_at<4>(O::shares, m.shares);
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::price, m.price);
    w.alpha_at(O::attribution, m.attribution);
}

template <>
void write_body(Writer& w, const OrderExecuted& m) {
    using O = OrderExecuted::Offset;
    w.be_at<8>(O::order_reference_number, m.order_reference_number);
    w.be_at<4>(O::executed_shares, m.executed_shares);
    w.be_at<8>(O::match_number, m.match_number);
}

template <>
void write_body(Writer& w, const OrderExecutedWithPrice& m) {
    using O = OrderExecutedWithPrice::Offset;
    w.be_at<8>(O::order_reference_number, m.order_reference_number);
    w.be_at<4>(O::executed_shares, m.executed_shares);
    w.be_at<8>(O::match_number, m.match_number);
    w.ch_at(O::printable, m.printable);
    w.be_at<4>(O::execution_price, m.execution_price);
}

template <>
void write_body(Writer& w, const OrderCancel& m) {
    using O = OrderCancel::Offset;
    w.be_at<8>(O::order_reference_number, m.order_reference_number);
    w.be_at<4>(O::cancelled_shares, m.cancelled_shares);
}

template <>
void write_body(Writer& w, const OrderDelete& m) {
    w.be_at<8>(OrderDelete::Offset::order_reference_number, m.order_reference_number);
}

template <>
void write_body(Writer& w, const OrderReplace& m) {
    using O = OrderReplace::Offset;
    w.be_at<8>(O::original_order_reference_number, m.original_order_reference_number);
    w.be_at<8>(O::new_order_reference_number, m.new_order_reference_number);
    w.be_at<4>(O::shares, m.shares);
    w.be_at<4>(O::price, m.price);
}

template <>
void write_body(Writer& w, const Trade& m) {
    using O = Trade::Offset;
    w.be_at<8>(O::order_reference_number, m.order_reference_number);
    w.ch_at(O::buy_sell_indicator, static_cast<char>(m.side));
    w.be_at<4>(O::shares, m.shares);
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::price, m.price);
    w.be_at<8>(O::match_number, m.match_number);
}

template <>
void write_body(Writer& w, const CrossTrade& m) {
    using O = CrossTrade::Offset;
    w.be_at<8>(O::shares, m.shares);
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::cross_price, m.cross_price);
    w.be_at<8>(O::match_number, m.match_number);
    w.ch_at(O::cross_type, m.cross_type);
}

template <>
void write_body(Writer& w, const BrokenTrade& m) {
    w.be_at<8>(BrokenTrade::Offset::match_number, m.match_number);
}

template <>
void write_body(Writer& w, const Noii& m) {
    using O = Noii::Offset;
    w.be_at<8>(O::paired_shares, m.paired_shares);
    w.be_at<8>(O::imbalance_shares, m.imbalance_shares);
    w.ch_at(O::imbalance_direction, m.imbalance_direction);
    w.alpha_at(O::stock, m.stock);
    w.be_at<4>(O::far_price, m.far_price);
    w.be_at<4>(O::near_price, m.near_price);
    w.be_at<4>(O::current_reference_price, m.current_reference_price);
    w.ch_at(O::cross_type, m.cross_type);
    w.ch_at(O::price_variation_indicator, m.price_variation_indicator);
}

template <>
void write_body(Writer& w, const RetailPriceImprovement& m) {
    using O = RetailPriceImprovement::Offset;
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::interest_flag, m.interest_flag);
}

template <>
void write_body(Writer& w, const DlcrPriceDiscovery& m) {
    using O = DlcrPriceDiscovery::Offset;
    w.alpha_at(O::stock, m.stock);
    w.ch_at(O::open_eligibility_status, m.open_eligibility_status);
    w.be_at<4>(O::minimum_allowable_price, m.minimum_allowable_price);
    w.be_at<4>(O::maximum_allowable_price, m.maximum_allowable_price);
    w.be_at<4>(O::near_execution_price, m.near_execution_price);
    w.be_at<8>(O::near_execution_time, m.near_execution_time);
    w.be_at<4>(O::lower_price_range_collar, m.lower_price_range_collar);
    w.be_at<4>(O::upper_price_range_collar, m.upper_price_range_collar);
}

} // namespace

std::size_t encode(const AnyMessage& msg, std::span<std::byte> out) {
    return std::visit(
        [out](const auto& m) -> std::size_t {
            using M = std::decay_t<decltype(m)>;
            if (out.size() < M::size) {
                throw std::length_error("ITCH encode: output buffer too small");
            }
            Writer w(out.first(M::size));
            w.header(M::type, m.header);
            write_body(w, m);
            return M::size;
        },
        msg);
}

void append_framed(const AnyMessage& msg, std::vector<std::byte>& out) {
    const std::size_t at = out.size();
    out.resize(at + frame_prefix_size + max_message_size);
    const std::size_t n = encode(msg, std::span(out).subspan(at + frame_prefix_size));
    out[at] = std::byte{static_cast<std::uint8_t>(n >> 8U)};
    out[at + 1] = std::byte{static_cast<std::uint8_t>(n & 0xFFU)};
    out.resize(at + frame_prefix_size + n);
}

std::vector<std::byte> encode_framed(std::span<const AnyMessage> msgs) {
    std::vector<std::byte> out;
    out.reserve(msgs.size() * (frame_prefix_size + 32));
    for (const auto& m : msgs) {
        append_framed(m, out);
    }
    return out;
}

} // namespace tickpath::itch
