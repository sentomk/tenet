#include <gtest/gtest.h>

#include <tenet/tenet.hpp>

TEST(SmokeTest, VersionMacrosAreConsistent) {
    EXPECT_EQ(TENET_VERSION_MAJOR, 0);
    EXPECT_EQ(TENET_VERSION, 0 * 10000 + 1 * 100 + 0);
}

TEST(SmokeTest, HeaderOnlyFlagIsDefined) {
#if TENET_HEADER_ONLY
    SUCCEED() << "header-only mode";
#else
    SUCCEED() << "compiled mode";
#endif
}
