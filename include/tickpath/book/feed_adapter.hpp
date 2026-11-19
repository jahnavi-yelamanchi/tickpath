#pragma once

// Glue from decoded ITCH messages to book operations. Header-only, no
// allocation, no virtual calls; usable as the parser's handler.

#include <optional>
#include <type_traits>
#include <variant>

#include "tickpath/book/concept.hpp"
#include "tickpath/itch/messages.hpp"

namespace tickpath::book {

/// ITCH handler that applies order messages to a book. Non-order messages
/// (system, directory, trades, imbalance, ...) do not change the book.
template <OrderBook B>
class BookUpdater {
public:
    explicit BookUpdater(B& book) noexcept
        : book_(&book) {}

    void on(const itch::AddOrder& m) noexcept {
        set(book_->add(m.header.stock_locate, m.order_reference_number, m.side, m.shares, m.price));
    }
    void on(const itch::AddOrderMpid& m) noexcept {
        set(book_->add(m.header.stock_locate, m.order_reference_number, m.side, m.shares, m.price));
    }
    void on(const itch::OrderExecuted& m) noexcept {
        set(book_->execute(m.header.stock_locate, m.order_reference_number, m.executed_shares));
    }
    /// Execution price may differ from the resting price; the order is reduced
    /// at its own price and no level moves.
    void on(const itch::OrderExecutedWithPrice& m) noexcept {
        set(book_->execute(m.header.stock_locate, m.order_reference_number, m.executed_shares));
    }
    void on(const itch::OrderCancel& m) noexcept {
        set(book_->cancel(m.header.stock_locate, m.order_reference_number, m.cancelled_shares));
    }
    void on(const itch::OrderDelete& m) noexcept {
        set(book_->remove(m.header.stock_locate, m.order_reference_number));
    }
    void on(const itch::OrderReplace& m) noexcept {
        set(book_->replace(m.header.stock_locate, m.original_order_reference_number,
                           m.new_order_reference_number, m.shares, m.price));
    }
    template <class M>
    void on(const M& /*non-order message*/) noexcept {
        last_.reset();
    }

    /// Status of the last order message, or nullopt if the last message did
    /// not touch the book.
    [[nodiscard]] std::optional<Status> last_status() const noexcept { return last_; }

private:
    void set(Status s) noexcept { last_ = s; }

    B* book_;
    std::optional<Status> last_;
};

/// True for message types that change the book.
template <class M>
inline constexpr bool is_order_message_v =
    std::is_same_v<M, itch::AddOrder> || std::is_same_v<M, itch::AddOrderMpid> ||
    std::is_same_v<M, itch::OrderExecuted> || std::is_same_v<M, itch::OrderExecutedWithPrice> ||
    std::is_same_v<M, itch::OrderCancel> || std::is_same_v<M, itch::OrderDelete> ||
    std::is_same_v<M, itch::OrderReplace>;

/// Applies one decoded message (tests, tools). Returns nullopt for messages
/// that do not change the book.
template <OrderBook B>
std::optional<Status> apply(B& book, const itch::AnyMessage& msg) {
    BookUpdater<B> u(book);
    std::visit([&u](const auto& m) { u.on(m); }, msg);
    return u.last_status();
}

} // namespace tickpath::book
