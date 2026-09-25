#include "wifi_interface.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace rauch
{
std::string select_first_wifi_interface(std::string_view nmcli_device_status)
{
    std::istringstream lines{ std::string(nmcli_device_status) };
    std::string        line;
    while (std::getline(lines, line))
    {
        const auto first_separator = line.find(':');
        if (first_separator == std::string::npos)
            continue;
        const auto second_separator = line.find(':', first_separator + 1);
        if (second_separator == std::string::npos)
            continue;
        const auto device = line.substr(0, first_separator);
        const auto type   = line.substr(first_separator + 1, second_separator - first_separator - 1);
        const auto state  = line.substr(second_separator + 1);
        if (device.empty() || type != "wifi" || state == "unavailable" || state == "unmanaged")
            continue;
        return device;
    }
    throw std::runtime_error("No usable Wi-Fi interface found by NetworkManager");
}
}  // namespace rauch
