#include "cli_options.hpp"

#include <charconv>
#include <stdexcept>

namespace rauch
{
std::uint16_t parse_tcp_port(std::string_view text)
{
    if (text.empty())
        throw std::invalid_argument("TCP port must be an integer from 0 to 65535");
    unsigned int port       = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (error != std::errc{} || end != text.data() + text.size() || port > 65535)
        throw std::invalid_argument("TCP port must be an integer from 0 to 65535");
    return static_cast<std::uint16_t>(port);
}
}  // namespace rauch
