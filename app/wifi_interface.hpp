#pragma once

#include <string>
#include <string_view>

namespace rauch
{
std::string select_first_wifi_interface(std::string_view nmcli_device_status);
}  // namespace rauch
