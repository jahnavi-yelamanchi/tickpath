// Instantiates the owner-written stubs inside a clang-tidy-checked target so
// the interfaces stay warning-clean before and after implementation.

#include <cstddef>
#include <span>

#include "tickpath/book/feed_adapter.hpp"
#include "tickpath/book/flat_book.hpp"
#include "tickpath/book/state_hash.hpp"
#include "tickpath/book/tree_book.hpp"
#include "tickpath/itch/handlers.hpp"
#include "tickpath/itch/parser.hpp"
#include "tickpath/risk/bbo.hpp"
#include "tickpath/risk/risk_check.hpp"

namespace tickpath {

static_assert(itch::Handler<itch::CountingHandler>);
static_assert(itch::Handler<book::BookUpdater<book::FlatBook>>);
static_assert(itch::Handler<book::BookUpdater<book::TreeBook>>);

template class book::BookUpdater<book::FlatBook>;
template class book::BookUpdater<book::TreeBook>;
template std::uint64_t book::state_hash<book::FlatBook>(const book::FlatBook&) noexcept;
template std::uint64_t book::state_hash<book::TreeBook>(const book::TreeBook&) noexcept;
template risk::Bbo risk::bbo_of<book::FlatBook>(const book::FlatBook&, Locate) noexcept;
template risk::Bbo risk::bbo_of<book::TreeBook>(const book::TreeBook&, Locate) noexcept;
template itch::FrameResult
itch::parse_frames<itch::CountingHandler>(std::span<const std::byte>,
                                          itch::CountingHandler&) noexcept;
template itch::FrameResult
itch::parse_frames<book::BookUpdater<book::FlatBook>>(std::span<const std::byte>,
                                                      book::BookUpdater<book::FlatBook>&) noexcept;
template itch::FrameResult
itch::parse_frames<book::BookUpdater<book::TreeBook>>(std::span<const std::byte>,
                                                      book::BookUpdater<book::TreeBook>&) noexcept;

} // namespace tickpath
