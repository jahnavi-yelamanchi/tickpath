// Explicit instantiations: compiles the header-only book code once inside a
// clang-tidy-checked target, and catches template errors without a test.

#include "tickpath/book/reference_book.hpp"

#include "tickpath/book/feed_adapter.hpp"
#include "tickpath/book/state_hash.hpp"

namespace tickpath::book {

template class BookUpdater<ReferenceBook>;
template std::uint64_t state_hash<ReferenceBook>(const ReferenceBook&) noexcept;
template std::uint64_t symbol_hash<ReferenceBook>(const ReferenceBook&, Locate) noexcept;

} // namespace tickpath::book
