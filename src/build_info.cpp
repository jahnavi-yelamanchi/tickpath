#include "tickpath/build_info.hpp"

#include "tickpath/detail/build_config.hpp"

namespace tickpath {

BuildInfo build_info() noexcept {
    return BuildInfo{
        .version = TICKPATH_VERSION,
        .compiler_id = TICKPATH_COMPILER_ID,
        .compiler_version = TICKPATH_COMPILER_VERSION,
        .build_type = TICKPATH_BUILD_TYPE,
        .sanitize = TICKPATH_SANITIZE,
    };
}

} // namespace tickpath
