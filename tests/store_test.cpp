#include "xf1_store.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <string>

namespace
{
rauch::Xf1Frame valid_frame()
{
    rauch::Xf1Frame frame{};
    frame[0]             = 206;
    frame[8]             = 3;
    frame[12]            = 1;
    const char product[] = "Urea";
    std::copy(std::begin(product), std::end(product) - 1, frame.begin() + 114);
    std::uint32_t checksum = 0;
    for (std::size_t i = 16; i < frame.size(); ++i)
        checksum += frame[i];
    frame[4] = static_cast<std::uint8_t>(checksum);
    frame[5] = static_cast<std::uint8_t>(checksum >> 8);
    return frame;
}
}  // namespace

TEST(Xf1FileStore, StoresCompleteFrameAndReturnsPath)
{
    const auto directory =
        std::filesystem::temp_directory_path()
        / ("rauch-xf1-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    rauch::Xf1FileStore store(directory);
    const auto          frame  = valid_frame();
    const auto          record = rauch::validate_and_decode(frame);

    const auto path = store.store(frame, record);
    EXPECT_EQ(path.filename(), "Urea.xf1");
    EXPECT_EQ(std::filesystem::file_size(path), rauch::xf1_size);
    const auto replacement = store.store(frame, record);
    EXPECT_EQ(replacement, path);
    EXPECT_EQ(std::filesystem::file_size(replacement), rauch::xf1_size);
    std::filesystem::remove_all(directory);
}
