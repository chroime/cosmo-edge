#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

extern "C" {
#include "libavutil/intfloat.h"
#include "libavutil/intreadwrite.h"
}

#include "catch_amalgamated.hpp"
#include "flow/stream/RtmpStreamPusher.h"
#include "util/UuidUtil.h"

TEST_CASE("RtmpStreamPusher rejects empty frames concurrently", "[rtmp][concurrency]") {
    const auto output_path =
        std::filesystem::path("/tmp") / ("cosmo-rtmp-pusher-" + cosmo::util::GenerateUUID() + ".flv");

    {
        cosmo::RtmpStreamPusher pusher(cosmo::media::VideoCodecType::kH264, output_path.string(), 640, 480,
                                       25.0F);
        uint8_t byte = 0;
        std::vector<std::thread> producers;
        for (int i = 0; i < 4; ++i) {
            producers.emplace_back([&pusher, &byte]() {
                for (int frame = 0; frame < 5; ++frame) {
                    pusher.PushFrame(nullptr, 1);
                    pusher.PushFrame(&byte, 0);
                }
            });
        }
        for (auto& producer : producers) {
            producer.join();
        }

        REQUIRE_FALSE(pusher.WaitReady(std::chrono::milliseconds(1)));
        REQUIRE(pusher.GetProcInfo().recvFrames == 0);
    }

    std::error_code error;
    std::filesystem::remove(output_path, error);
}

TEST_CASE("RtmpStreamPusher accepts a valid small startup keyframe", "[rtmp][regression]") {
    const auto [width, height, fps, expected_width, expected_height, expected_fps] =
        GENERATE(table<int, int, float, double, double, double>({{640, 480, 25.0F, 640, 480, 25},
                                                                 {1920, 1088, 30.0F, 1920, 1088, 30},
                                                                 {1921, 720, 30.0F, 1920, 1080, 30},
                                                                 {1280, 1089, 30.0F, 1920, 1080, 30},
                                                                 {16, 16, 1.0F, 1920, 1080, 1},
                                                                 {640, 480, 0.0F, 1920, 1080, 25},
                                                                 {640, 480, 0.05F, 1920, 1080, 0.05}}));
    CAPTURE(width, height, fps);
    const auto output_path =
        std::filesystem::path("/tmp") / ("cosmo-rtmp-small-keyframe-" + cosmo::util::GenerateUUID() + ".flv");

    // Decodable Annex-B H.264 access unit generated from one 16x16 black frame
    // (SPS, PPS and IDR). Small keyframes are valid for low-resolution or
    // low-complexity scenes and must not be rejected by an arbitrary threshold.
    const std::vector<std::uint8_t> small_keyframe{
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0a, 0xda, 0x7b, 0x01, 0x10, 0x00, 0x00, 0x03, 0x00,
        0x10, 0x00, 0x00, 0x03, 0x00, 0x28, 0xf1, 0x22, 0x6a, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x0f,
        0xc8, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x3a, 0x26, 0x28, 0x00, 0x09, 0x02, 0xe0,
    };
    REQUIRE(small_keyframe.size() < 1024);

    {
        cosmo::RtmpStreamPusher pusher(cosmo::media::VideoCodecType::kH264, output_path.string(), width,
                                       height, fps);
        SECTION("constructor attributes") {}
        SECTION("attribute updates preserve the established codec configuration") {
            pusher.SetVideoAttr(1280, 720, 60.0F);
            pusher.SetVideoAttr(0, 0, 0.0F);
        }
        pusher.PushFrame(small_keyframe.data(), small_keyframe.size());

        REQUIRE(pusher.WaitReady(std::chrono::milliseconds(100)));
        CHECK(pusher.GetProcInfo().recvFrames == 1);
        CHECK(pusher.GetProcInfo().sendFrames == 1);

        pusher.Stop();
        pusher.Stop();
        pusher.PushFrame(small_keyframe.data(), small_keyframe.size());
        CHECK_FALSE(pusher.IsReady());
        CHECK_FALSE(pusher.WaitReady(std::chrono::milliseconds(1)));
        CHECK(pusher.GetProcInfo().recvFrames == 1);
        CHECK(pusher.GetProcInfo().sendFrames == 1);
    }

    REQUIRE(std::filesystem::exists(output_path));
    CHECK(std::filesystem::file_size(output_path) > 0);
    std::ifstream input(output_path, std::ios::binary);
    const std::string output{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto check_metadata = [&](const std::string& name, double value) {
        // Check AMF number entries in the emitted FLV header, before a decoder
        // can replace its dimensions with the tiny frame's SPS dimensions.
        std::string entry(2, '\0');
        AV_WB16(entry.data(), name.size());
        entry += name;
        entry.push_back('\0');
        uint8_t number[8];
        AV_WB64(number, av_double2int(value));
        entry.append(reinterpret_cast<const char*>(number), sizeof(number));
        INFO("FLV metadata: " << name << "=" << value);
        CHECK(output.find(entry) != std::string::npos);
    };
    check_metadata("width", expected_width);
    check_metadata("height", expected_height);
    check_metadata("framerate", expected_fps);
    std::error_code error;
    std::filesystem::remove(output_path, error);
}
