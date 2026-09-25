#include "can_transport.hpp"

#include <memory>
#include <utility>

namespace rauch
{
std::unique_ptr<ICanTransport> create_platform_can_transport(std::string interface_name)
{
    return std::make_unique<UnsupportedCanTransport>("SocketCAN is unavailable on macOS: " + interface_name);
}
}  // namespace rauch
