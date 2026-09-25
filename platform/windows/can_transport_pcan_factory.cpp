#include "can_transport.hpp"
#include "pcan_transport.hpp"

#include <PCANBasic.h>
#include <charconv>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace rauch
{
namespace
{
    TPCANHandle parse_channel(std::string_view interface_name)
    {
        constexpr std::string_view prefix = "PCAN_USBBUS";
        if (interface_name == "can0")
            return PCAN_USBBUS1;
        if (interface_name.size() < prefix.size() || interface_name.substr(0, prefix.size()) != prefix)
            throw std::invalid_argument("PCAN interface must be can0 or PCAN_USBBUS1..PCAN_USBBUS16");

        unsigned int channel_number = 0;
        const auto   suffix         = interface_name.substr(prefix.size());
        const auto [end, error]     = std::from_chars(suffix.data(), suffix.data() + suffix.size(), channel_number);
        if (error != std::errc{} || end != suffix.data() + suffix.size() || channel_number < 1 || channel_number > 16)
            throw std::invalid_argument("PCAN USB channel number must be between 1 and 16");

        return static_cast<TPCANHandle>(PCAN_USBBUS1 + channel_number - 1);
    }
}  // namespace

std::unique_ptr<ICanTransport> create_platform_can_transport(std::string interface_name)
{
    return std::make_unique<PcanCanTransport>(parse_channel(interface_name), PCAN_BAUD_250K);
}
}  // namespace rauch
