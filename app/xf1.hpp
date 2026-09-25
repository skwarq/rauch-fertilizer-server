#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace rauch
{
inline constexpr std::size_t xf1_size = 206;
using Xf1Frame                        = std::array<std::uint8_t, xf1_size>;

struct Xf1Record
{
    std::string manufacturer;
    std::string product;
};

Xf1Record             validate_and_decode(const Xf1Frame& frame);
std::filesystem::path safe_filename(const Xf1Record& record);
}  // namespace rauch
