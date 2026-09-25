#pragma once

#include <cstdint>
#include <string_view>

namespace rauch
{
std::uint16_t parse_tcp_port(std::string_view text);
}  // namespace rauch
