#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "tickpath/build_info.hpp"

TEST(Build, InfoIsPopulated) {
    const auto info = tickpath::build_info();
    EXPECT_FALSE(info.version.empty());
    EXPECT_FALSE(info.compiler_id.empty());
}

RC_GTEST_PROP(Build, RapidCheckIsWired, (int x)) {
    RC_ASSERT(x == x);
}
