#include "tickpath/sim/generator.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tickpath/sim/rng.hpp"

namespace tickpath::sim {
namespace {

using itch::AnyMessage;
using itch::Header;
using itch::Locate;
using itch::OrderRef;
using itch::Price4;
using itch::Shares;
using itch::Side;
using itch::Stock;

constexpr itch::Timestamp market_open_ns = 34'200ULL * 1'000'000'000ULL; // 09:30:00

Stock stock_name(std::string_view s) {
    Stock out{};
    out.fill(' ');
    for (std::size_t i = 0; i < s.size() && i < out.size(); ++i) {
        out[i] = s[i];
    }
    return out;
}

Stock symbol_for(Locate locate) {
    // SYM0001 .. SYM9999; deterministic, unique per locate.
    const std::string s = "SYM" + std::to_string(locate);
    return stock_name(s);
}

/// Stamps headers: increasing timestamps and tracking numbers.
class Clock {
public:
    explicit Clock(itch::Timestamp start) noexcept
        : now_(start) {}

    Header next(Locate locate, itch::Timestamp step = 1'000) noexcept {
        now_ += step;
        ++tracking_;
        return Header{.stock_locate = locate, .tracking_number = tracking_, .timestamp = now_};
    }

private:
    itch::Timestamp now_;
    std::uint16_t tracking_ = 0;
};

itch::StockDirectory directory(Header h, Stock stock) {
    itch::StockDirectory m{};
    m.header = h;
    m.stock = stock;
    m.market_category = 'Q';
    m.financial_status_indicator = 'N';
    m.round_lot_size = 100;
    m.round_lots_only = 'N';
    m.issue_classification = 'C';
    m.issue_sub_type = itch::alpha<2>("Z");
    m.authenticity = 'P';
    m.short_sale_threshold_indicator = 'N';
    m.ipo_flag = 'N';
    m.luld_reference_price_tier = '1';
    m.etp_flag = 'N';
    m.etp_leverage_factor = 0;
    m.inverse_indicator = 'N';
    return m;
}

itch::StockTradingAction trading_action(Header h, Stock stock, char state) {
    return itch::StockTradingAction{.header = h,
                                    .stock = stock,
                                    .trading_state = state,
                                    .reserved = ' ',
                                    .reason = itch::alpha<4>("")};
}

// ---- Random day -------------------------------------------------------------

struct LiveOrder {
    Locate locate{};
    Side side{};
    Price4 price{};
    Shares shares{};
    std::size_t slot{}; ///< index in the symbol's live-id vector
};

struct SymbolState {
    Stock stock{};
    Price4 mid{};
    std::vector<OrderRef> live; ///< live ids, for O(1) uniform pick
};

class RandomDay {
public:
    explicit RandomDay(const GeneratorConfig& cfg)
        : cfg_(cfg)
        , rng_(cfg.seed)
        , clock_(market_open_ns - 3'600ULL * 1'000'000'000ULL) {
        syms_.resize(static_cast<std::size_t>(cfg.symbols) + 1U);
        for (Locate l = 1; l <= cfg.symbols; ++l) {
            auto& s = syms_[l];
            s.stock = symbol_for(l);
            // mid between $10 and $500, on a tick
            s.mid = static_cast<Price4>(rng_.between(1'000, 50'000) * cfg.tick);
        }
    }

    std::vector<AnyMessage> run() {
        out_.reserve(cfg_.book_messages + std::size_t{8} * cfg_.symbols + 16U);
        preamble();
        for (std::size_t i = 0; i < cfg_.book_messages; ++i) {
            step();
        }
        epilogue();
        return std::move(out_);
    }

private:
    Locate pick_symbol() { return static_cast<Locate>(rng_.between(1, cfg_.symbols)); }

    Shares pick_shares() {
        if (rng_.chance(1, 10)) {
            return static_cast<Shares>(rng_.between(1, 99)); // odd lot
        }
        return static_cast<Shares>(100U * rng_.between(1, 20));
    }

    Price4 pick_price(Locate l, Side side) {
        const auto off = static_cast<Price4>(rng_.between(1, cfg_.depth_ticks) * cfg_.tick);
        const Price4 mid = syms_[l].mid;
        return side == Side::Buy ? mid - off : mid + off;
    }

    void preamble() {
        out_.emplace_back(itch::SystemEvent{.header = clock_.next(0), .event_code = 'O'});
        for (Locate l = 1; l <= cfg_.symbols; ++l) {
            out_.emplace_back(directory(clock_.next(l), syms_[l].stock));
            out_.emplace_back(itch::RegShoRestriction{
                .header = clock_.next(l), .stock = syms_[l].stock, .reg_sho_action = '0'});
            out_.emplace_back(trading_action(clock_.next(l), syms_[l].stock, 'T'));
        }
        out_.emplace_back(itch::MwcbDeclineLevel{.header = clock_.next(0),
                                                 .level1 = 400'000'000'000ULL,
                                                 .level2 = 370'000'000'000ULL,
                                                 .level3 = 320'000'000'000ULL});
        out_.emplace_back(itch::SystemEvent{.header = clock_.next(0), .event_code = 'S'});
        out_.emplace_back(itch::SystemEvent{.header = clock_.next(0, 3'600ULL * 1'000'000'000ULL),
                                            .event_code = 'Q'});
    }

    void epilogue() {
        for (Locate l = 1; l <= cfg_.symbols; ++l) {
            out_.emplace_back(itch::CrossTrade{.header = clock_.next(l),
                                               .shares = rng_.between(0, 100'000),
                                               .stock = syms_[l].stock,
                                               .cross_price = syms_[l].mid,
                                               .match_number = ++match_,
                                               .cross_type = 'C'});
        }
        out_.emplace_back(itch::SystemEvent{.header = clock_.next(0), .event_code = 'M'});
        out_.emplace_back(itch::SystemEvent{.header = clock_.next(0), .event_code = 'E'});
        out_.emplace_back(itch::SystemEvent{.header = clock_.next(0), .event_code = 'C'});
    }

    void step() {
        const Locate l = pick_symbol();
        auto& sym = syms_[l];
        const auto r = rng_.below(100);
        if (sym.live.empty() || r < 40) {
            if (sym.live.size() >= cfg_.max_live_orders_per_symbol) {
                remove_random(l);
            } else {
                add(l, r < 4);
            }
        } else if (r < 52) {
            execute(l, false);
        } else if (r < 55) {
            execute(l, true);
        } else if (r < 65) {
            cancel(l);
        } else if (r < 83) {
            remove_random(l);
        } else if (r < 93) {
            replace(l);
        } else if (r < 97) {
            non_displayed_trade(l);
        } else if (r < 99) {
            imbalance(l);
        } else {
            out_.emplace_back(itch::RetailPriceImprovement{
                .header = clock_.next(l), .stock = sym.stock, .interest_flag = 'B'});
        }
    }

    OrderRef pick_live(Locate l) {
        const auto& live = syms_[l].live;
        return live[rng_.below(live.size())];
    }

    void track_add(OrderRef id, Locate l, Side side, Price4 px, Shares qty) {
        auto& live = syms_[l].live;
        orders_.emplace(
            id,
            LiveOrder{.locate = l, .side = side, .price = px, .shares = qty, .slot = live.size()});
        live.push_back(id);
    }

    void track_remove(OrderRef id) {
        const auto it = orders_.find(id);
        auto& live = syms_[it->second.locate].live;
        const std::size_t slot = it->second.slot;
        live[slot] = live.back();
        orders_.at(live[slot]).slot = slot;
        live.pop_back();
        orders_.erase(it);
    }

    void add(Locate l, bool with_mpid) {
        const OrderRef id = next_id_++;
        const Side side = rng_.chance(1, 2) ? Side::Buy : Side::Sell;
        const Price4 px = pick_price(l, side);
        const Shares qty = pick_shares();
        const Header h = clock_.next(l, rng_.between(1, 50'000));
        if (with_mpid) {
            out_.emplace_back(itch::AddOrderMpid{.header = h,
                                                 .order_reference_number = id,
                                                 .side = side,
                                                 .shares = qty,
                                                 .stock = syms_[l].stock,
                                                 .price = px,
                                                 .attribution = itch::alpha<4>("MMKR")});
        } else {
            out_.emplace_back(itch::AddOrder{.header = h,
                                             .order_reference_number = id,
                                             .side = side,
                                             .shares = qty,
                                             .stock = syms_[l].stock,
                                             .price = px});
        }
        track_add(id, l, side, px, qty);
    }

    void execute(Locate l, bool with_price) {
        const OrderRef id = pick_live(l);
        auto& o = orders_.at(id);
        const bool full = rng_.chance(1, 3);
        const Shares qty = full ? o.shares : static_cast<Shares>(rng_.between(1, o.shares));
        const Header h = clock_.next(l, rng_.between(1, 50'000));
        if (with_price) {
            // Execution price may differ from the resting price (e.g. price
            // improvement); the book still reduces the order at its own price.
            const auto delta = static_cast<Price4>(rng_.below(3) * cfg_.tick);
            const Price4 px = o.side == Side::Buy ? o.price - delta : o.price + delta;
            out_.emplace_back(
                itch::OrderExecutedWithPrice{.header = h,
                                             .order_reference_number = id,
                                             .executed_shares = qty,
                                             .match_number = ++match_,
                                             .printable = rng_.chance(1, 4) ? 'N' : 'Y',
                                             .execution_price = px});
        } else {
            out_.emplace_back(itch::OrderExecuted{.header = h,
                                                  .order_reference_number = id,
                                                  .executed_shares = qty,
                                                  .match_number = ++match_});
        }
        reduce(id, qty);
    }

    void cancel(Locate l) {
        const OrderRef id = pick_live(l);
        const auto& o = orders_.at(id);
        const auto qty = static_cast<Shares>(rng_.between(1, o.shares)); // may cancel all
        out_.emplace_back(itch::OrderCancel{.header = clock_.next(l, rng_.between(1, 50'000)),
                                            .order_reference_number = id,
                                            .cancelled_shares = qty});
        reduce(id, qty);
    }

    void reduce(OrderRef id, Shares qty) {
        auto& o = orders_.at(id);
        o.shares -= qty;
        if (o.shares == 0) {
            track_remove(id);
        }
    }

    void remove_random(Locate l) {
        const OrderRef id = pick_live(l);
        out_.emplace_back(itch::OrderDelete{.header = clock_.next(l, rng_.between(1, 50'000)),
                                            .order_reference_number = id});
        track_remove(id);
    }

    void replace(Locate l) {
        const OrderRef old_id = pick_live(l);
        const LiveOrder old = orders_.at(old_id);
        const OrderRef new_id = next_id_++;
        const Price4 px = rng_.chance(1, 3) ? old.price : pick_price(l, old.side);
        const Shares qty = pick_shares();
        out_.emplace_back(itch::OrderReplace{.header = clock_.next(l, rng_.between(1, 50'000)),
                                             .original_order_reference_number = old_id,
                                             .new_order_reference_number = new_id,
                                             .shares = qty,
                                             .price = px});
        track_remove(old_id);
        track_add(new_id, l, old.side, px, qty);
    }

    void non_displayed_trade(Locate l) {
        out_.emplace_back(itch::Trade{.header = clock_.next(l),
                                      .order_reference_number = 0,
                                      .side = Side::Buy,
                                      .shares = pick_shares(),
                                      .stock = syms_[l].stock,
                                      .price = syms_[l].mid,
                                      .match_number = ++match_});
    }

    void imbalance(Locate l) {
        const Price4 mid = syms_[l].mid;
        out_.emplace_back(itch::Noii{.header = clock_.next(l),
                                     .paired_shares = rng_.between(0, 1'000'000),
                                     .imbalance_shares = rng_.between(0, 100'000),
                                     .imbalance_direction = rng_.chance(1, 2) ? 'B' : 'S',
                                     .stock = syms_[l].stock,
                                     .far_price = mid,
                                     .near_price = mid,
                                     .current_reference_price = mid,
                                     .cross_type = 'C',
                                     .price_variation_indicator = 'L'});
    }

    GeneratorConfig cfg_;
    Rng rng_;
    Clock clock_;
    std::vector<SymbolState> syms_;
    std::unordered_map<OrderRef, LiveOrder> orders_;
    std::vector<AnyMessage> out_;
    OrderRef next_id_ = 1;
    itch::MatchNumber match_ = 0;
};

// ---- Directed scenarios -----------------------------------------------------

class Script {
public:
    Script()
        : clock_(market_open_ns) {}

    Header h(Locate l) { return clock_.next(l); }

    void add(Locate l, OrderRef id, Side side, Shares qty, Price4 px) {
        out.emplace_back(itch::AddOrder{.header = h(l),
                                        .order_reference_number = id,
                                        .side = side,
                                        .shares = qty,
                                        .stock = symbol_for(l),
                                        .price = px});
    }
    void exec(Locate l, OrderRef id, Shares qty) {
        out.emplace_back(itch::OrderExecuted{.header = h(l),
                                             .order_reference_number = id,
                                             .executed_shares = qty,
                                             .match_number = ++match_});
    }
    void exec_px(Locate l, OrderRef id, Shares qty, Price4 px, char printable) {
        out.emplace_back(itch::OrderExecutedWithPrice{.header = h(l),
                                                      .order_reference_number = id,
                                                      .executed_shares = qty,
                                                      .match_number = ++match_,
                                                      .printable = printable,
                                                      .execution_price = px});
    }
    void cancel(Locate l, OrderRef id, Shares qty) {
        out.emplace_back(itch::OrderCancel{
            .header = h(l), .order_reference_number = id, .cancelled_shares = qty});
    }
    void del(Locate l, OrderRef id) {
        out.emplace_back(itch::OrderDelete{.header = h(l), .order_reference_number = id});
    }
    void replace(Locate l, OrderRef old_id, OrderRef new_id, Shares qty, Price4 px) {
        out.emplace_back(itch::OrderReplace{.header = h(l),
                                            .original_order_reference_number = old_id,
                                            .new_order_reference_number = new_id,
                                            .shares = qty,
                                            .price = px});
    }
    void open_day(std::initializer_list<Locate> locates) {
        out.emplace_back(itch::SystemEvent{.header = h(0), .event_code = 'O'});
        for (const Locate l : locates) {
            out.emplace_back(directory(h(l), symbol_for(l)));
            out.emplace_back(trading_action(h(l), symbol_for(l), 'T'));
        }
        out.emplace_back(itch::SystemEvent{.header = h(0), .event_code = 'S'});
        out.emplace_back(itch::SystemEvent{.header = h(0), .event_code = 'Q'});
    }
    void close_day() {
        out.emplace_back(itch::SystemEvent{.header = h(0), .event_code = 'M'});
        out.emplace_back(itch::SystemEvent{.header = h(0), .event_code = 'E'});
        out.emplace_back(itch::SystemEvent{.header = h(0), .event_code = 'C'});
    }
    itch::MatchNumber next_match() { return ++match_; }

    std::vector<AnyMessage> out;

private:
    Clock clock_;
    itch::MatchNumber match_ = 0;
};

constexpr Locate loc_a = 1;
constexpr Locate loc_b = 2;
constexpr Locate loc_max = std::numeric_limits<Locate>::max();
constexpr Price4 px_10 = 100'000; // $10.0000

} // namespace

std::vector<AnyMessage> generate_random(const GeneratorConfig& cfg) {
    return RandomDay(cfg).run();
}

std::vector<AnyMessage> generate_edge_cases() {
    Script s;
    s.open_day({loc_a, loc_b, loc_max});

    // Every non-book message type.
    const Stock a = symbol_for(loc_a);
    s.out.emplace_back(
        itch::RegShoRestriction{.header = s.h(loc_a), .stock = a, .reg_sho_action = '1'});
    s.out.emplace_back(itch::MarketParticipantPosition{.header = s.h(loc_a),
                                                       .mpid = itch::alpha<4>("MMKR"),
                                                       .stock = a,
                                                       .primary_market_maker = 'Y',
                                                       .market_maker_mode = 'N',
                                                       .market_participant_state = 'A'});
    s.out.emplace_back(itch::MwcbDeclineLevel{.header = s.h(0),
                                              .level1 = 400'000'000'000ULL,
                                              .level2 = 370'000'000'000ULL,
                                              .level3 = 320'000'000'000ULL});
    s.out.emplace_back(itch::MwcbStatus{.header = s.h(0), .breached_level = '1'});
    s.out.emplace_back(itch::IpoQuotingPeriodUpdate{.header = s.h(loc_b),
                                                    .stock = symbol_for(loc_b),
                                                    .ipo_quotation_release_time = 36'000,
                                                    .ipo_quotation_release_qualifier = 'A',
                                                    .ipo_price = 200'000});
    s.out.emplace_back(itch::LuldAuctionCollar{.header = s.h(loc_a),
                                               .stock = a,
                                               .auction_collar_reference_price = px_10,
                                               .upper_auction_collar_price = 110'000,
                                               .lower_auction_collar_price = 90'000,
                                               .auction_collar_extension = 1});
    s.out.emplace_back(itch::OperationalHalt{
        .header = s.h(loc_a), .stock = a, .market_code = 'Q', .operational_halt_action = 'T'});

    // Bids: two orders at one level (FIFO), one level below.
    s.add(loc_a, 1, Side::Buy, 100, px_10);
    s.add(loc_a, 2, Side::Buy, 200, px_10);
    s.add(loc_a, 3, Side::Buy, 300, px_10 - 100);
    // Asks, one with MPID attribution.
    s.out.emplace_back(itch::AddOrderMpid{.header = s.h(loc_a),
                                          .order_reference_number = 4,
                                          .side = Side::Sell,
                                          .shares = 100,
                                          .stock = a,
                                          .price = px_10 + 100,
                                          .attribution = itch::alpha<4>("MMKR")});
    s.add(loc_a, 5, Side::Sell, 50, px_10 + 200);

    s.exec(loc_a, 1, 40);                      // partial execution
    s.exec(loc_a, 1, 60);                      // full execution; level keeps order 2
    s.exec_px(loc_a, 2, 50, px_10 + 100, 'Y'); // partial, at a different price
    s.exec_px(loc_a, 2, 150, px_10, 'N');      // full, non-printable; best-bid level empties
    s.cancel(loc_a, 3, 100);                   // partial cancel
    s.cancel(loc_a, 3, 200);                   // cancel of all remaining shares; bid side empty
    s.del(loc_a, 5);                           // delete; ask level empties
    s.add(loc_a, 7, Side::Sell, 100, px_10 + 100);
    s.replace(loc_a, 4, 6, 300, px_10 + 100); // same price: 6 queues behind 7
    s.replace(loc_a, 6, 8, 500, px_10 + 300); // new price, larger size

    // FIFO: delete from the middle of a level.
    s.add(loc_a, 12, Side::Buy, 100, 99'000);
    s.add(loc_a, 13, Side::Buy, 100, 99'000);
    s.add(loc_a, 14, Side::Buy, 100, 99'000);
    s.del(loc_a, 13);

    // Extreme prices on one symbol: $0.0001 and $200,000.0000.
    s.add(loc_b, 9, Side::Buy, 100, 1);
    s.add(loc_b, 10, Side::Sell, 100, 2'000'000'000);
    s.exec(loc_b, 9, 100);

    // Highest locate, order refs 0 and 2^64-1.
    s.add(loc_max, 0, Side::Buy, 100, 50'000);
    s.add(loc_max, std::numeric_limits<OrderRef>::max(), Side::Buy, 100, 49'900);
    s.del(loc_max, 0);
    s.del(loc_max, std::numeric_limits<OrderRef>::max());

    // Trade, cross, broken trade, imbalance, RPII, DLCR.
    s.out.emplace_back(itch::Trade{.header = s.h(loc_a),
                                   .order_reference_number = 0,
                                   .side = Side::Buy,
                                   .shares = 100,
                                   .stock = a,
                                   .price = px_10,
                                   .match_number = s.next_match()});
    s.out.emplace_back(itch::CrossTrade{.header = s.h(loc_a),
                                        .shares = 1'000,
                                        .stock = a,
                                        .cross_price = px_10,
                                        .match_number = s.next_match(),
                                        .cross_type = 'O'});
    s.out.emplace_back(itch::BrokenTrade{.header = s.h(loc_a), .match_number = 1});
    s.out.emplace_back(itch::Noii{.header = s.h(loc_a),
                                  .paired_shares = 1'000,
                                  .imbalance_shares = 200,
                                  .imbalance_direction = 'B',
                                  .stock = a,
                                  .far_price = px_10,
                                  .near_price = px_10,
                                  .current_reference_price = px_10,
                                  .cross_type = 'C',
                                  .price_variation_indicator = ' '});
    s.out.emplace_back(
        itch::RetailPriceImprovement{.header = s.h(loc_a), .stock = a, .interest_flag = 'A'});
    s.out.emplace_back(itch::DlcrPriceDiscovery{.header = s.h(loc_b),
                                                .stock = symbol_for(loc_b),
                                                .open_eligibility_status = 'Y',
                                                .minimum_allowable_price = 90'000,
                                                .maximum_allowable_price = 110'000,
                                                .near_execution_price = px_10,
                                                .near_execution_time = market_open_ns,
                                                .lower_price_range_collar = 95'000,
                                                .upper_price_range_collar = 105'000});
    s.out.emplace_back(trading_action(s.h(loc_a), a, 'H'));

    s.close_day();
    return std::move(s.out);
}

std::vector<AnyMessage> generate_contract_violations() {
    Script s;
    s.open_day({loc_a, loc_b});

    s.add(loc_a, 100, Side::Buy, 0, px_10); // zero-quantity add
    s.add(loc_a, 101, Side::Buy, 100, px_10);
    s.add(loc_a, 101, Side::Buy, 100, px_10); // duplicate id
    s.exec(loc_a, 999, 10);                   // unknown id: E, C, X, D, U
    s.exec_px(loc_a, 999, 10, px_10, 'Y');
    s.cancel(loc_a, 999, 10);
    s.del(loc_a, 999);
    s.replace(loc_a, 999, 1'000, 100, px_10);
    s.exec(loc_a, 101, 0);                    // zero-quantity execute
    s.cancel(loc_a, 101, 0);                  // zero-quantity cancel
    s.exec(loc_a, 101, 150);                  // overfill: 150 of 100
    s.add(loc_a, 101, Side::Buy, 100, px_10); // id reuse after removal

    s.add(loc_a, 102, Side::Sell, 100, px_10 + 100);
    s.add(loc_a, 103, Side::Sell, 100, px_10 + 200);
    s.replace(loc_a, 102, 103, 100, px_10 + 100); // new id already live
    s.replace(loc_a, 103, 103, 100, px_10 + 100); // new id == old id
    s.replace(loc_a, 102, 104, 0, px_10 + 100);   // replace to zero shares
    s.exec(loc_b, 102, 10);                       // locate mismatch: E, X, D, U
    s.cancel(loc_b, 102, 10);
    s.del(loc_b, 102);
    s.replace(loc_b, 102, 105, 100, px_10);
    s.cancel(loc_a, 102, 500); // cancel overfill
    s.del(loc_a, 102);         // already removed

    s.add(loc_a, 106, Side::Buy, 100, px_10 + 300); // crossed: bid above best ask
    s.exec(loc_a, 106, 100);

    s.close_day();
    return std::move(s.out);
}

} // namespace tickpath::sim
