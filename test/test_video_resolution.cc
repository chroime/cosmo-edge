#include <limits>

#include "catch_amalgamated.hpp"
#include "util/VideoInfo.h"

namespace cosmo::media {

TEST_CASE("Video admission accepts sub-720p streams", "[media][video-resolution]") {
    CHECK(IsValidVideoResolution(320, 240));
    CHECK(IsValidVideoResolution(640, 360));
    CHECK(IsValidVideoResolution(640, 480));
    CHECK(IsValidVideoResolution(704, 576));
    CHECK(IsValidVideoResolution(1280, 720));
    CHECK(IsValidVideoResolution(1920, 1080));
    CHECK(IsValidVideoResolution(3840, 2160));
    CHECK(IsValidVideoResolution(7680, 4320));
}

TEST_CASE("Video admission enforces per-axis frame boundaries", "[media][video-resolution][security]") {
    CHECK(IsValidVideoResolution(64, 64));
    CHECK(IsValidVideoResolution(128, 64));
    CHECK_FALSE(IsValidVideoResolution(62, 720));
    CHECK_FALSE(IsValidVideoResolution(1280, 62));
    CHECK_FALSE(IsValidVideoResolution(32, 32));
    CHECK_FALSE(IsValidVideoResolution(0, 720));
    CHECK_FALSE(IsValidVideoResolution(1280, 0));
    CHECK_FALSE(IsValidVideoResolution(-1280, -720));
    CHECK_FALSE(IsValidVideoResolution(641, 480));
    CHECK_FALSE(IsValidVideoResolution(640, 481));
    CHECK_FALSE(IsValidVideoResolution(7682, 480));
    CHECK_FALSE(IsValidVideoResolution(640, 4322));
    CHECK_FALSE(IsValidVideoResolution(std::numeric_limits<int>::max(), 720));
    CHECK_FALSE(IsValidVideoResolution(1280, std::numeric_limits<int>::max()));
    CHECK_FALSE(IsValidVideoResolution(std::numeric_limits<int>::min(), -720));
    // This used to wrap to an accepted pixel count on common platforms.
    CHECK_FALSE(IsValidVideoResolution(65536, 65600));
}

}  // namespace cosmo::media
