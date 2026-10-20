#pragma once

#include <cstddef>

namespace cosmo::media {

constexpr int kQueueSizeCoefficient = 1;  // Queue length coefficient

constexpr int kPictureMaxWidth  = 4096;
constexpr int kPictureMaxHeight = 4096;
constexpr int kPictureMinWidth  = 40;
constexpr int kPictureMinHeight = 40;

constexpr bool IsValidPicture(int width, int height) {
    return (width >= kPictureMinWidth) && (height >= kPictureMinHeight) &&
           ((width * height) <= (kPictureMaxWidth * kPictureMaxHeight));
}

constexpr int kVideo4KWidth  = 7680;
constexpr int kVideo4KHeight = 4320;

constexpr int kVideoMaxWidth  = kVideo4KWidth;
constexpr int kVideoMaxHeight = kVideo4KHeight;

constexpr int kVideoFrameMaxSize = kVideoMaxWidth * kVideoMaxHeight * 3;

constexpr int kVideoDefaultWidth       = 1920;
constexpr int kVideoDefaultHeight      = 1088;
constexpr int kVideoDefaultHeightAlign = 1088;

// 64x64 is the smallest resolution verified through BM1688 RTSP live preview.
constexpr int kVideoMinWidth  = 64;
constexpr int kVideoMinHeight = 64;

constexpr bool IsValidVideoWidth(int value) {
    return (value >= kVideoMinWidth) && (value <= kVideoMaxWidth) && (value % 2 == 0);
}

constexpr bool IsValidVideoHeight(int value) {
    return (value >= kVideoMinHeight) && (value <= kVideoMaxHeight) && (value % 2 == 0);
}

constexpr int kVideoInfoMaxDuration = 10 * 1000;  // 10 seconds

constexpr int kTimestampDiff = 1000;  // Video timestamp diff

constexpr bool IsValidVideoResolution(int width, int height) {
    // Validate each dimension without multiplying untrusted signed integers.
    // Decoded YUV420 frames require even width and height.
    return IsValidVideoWidth(width) && IsValidVideoHeight(height);
}

constexpr double kFpsCtrlMaxFps = 1000000.0;  // Used when external FPS setting < 0

}  // namespace cosmo::media
