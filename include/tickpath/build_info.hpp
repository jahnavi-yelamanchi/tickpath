#pragma once

#include <string_view>

namespace tickpath {

/// Build metadata embedded in benchmark output so results carry their provenance.
struct BuildInfo {
    std::string_view version;
    std::string_view compiler_id;
    std::string_view compiler_version;
    std::string_view build_type;
    std::string_view sanitize;
};

[[nodiscard]] BuildInfo build_info() noexcept;

} // namespace tickpath
