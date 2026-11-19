// libFuzzer target for the ITCH parser. Checks memory safety (ASan/UBSan) and
// the result invariants documented in parser.hpp.

#include <cstddef>
#include <cstdint>
#include <span>

#include "tickpath/itch/handlers.hpp"
#include "tickpath/itch/parser.hpp"

namespace {

void require(bool ok) {
    if (!ok) {
        __builtin_trap();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::span<const std::byte> buf(reinterpret_cast<const std::byte*>(data), size);
    using tickpath::itch::ParseStatus;

    tickpath::itch::CountingHandler frames;
    const auto r = tickpath::itch::parse_frames(buf, frames);
    require(r.consumed <= size);
    require(r.messages == frames.total);
    require(r.status != ParseStatus::Ok || r.consumed == size);

    tickpath::itch::CountingHandler one;
    const auto s = tickpath::itch::parse_message(buf, one);
    require(one.total == (s == ParseStatus::Ok ? 1U : 0U));
    require(s != ParseStatus::Ok ||
            size == tickpath::itch::message_size(static_cast<char>(data[0])));
    return 0;
}
