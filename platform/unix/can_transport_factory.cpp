#include "socketcan_transport.hpp"

#include <memory>
#include <string>

namespace rauch
{
std::unique_ptr<ICanTransport> create_platform_can_transport(std::string interface_name)
{
    return std::make_unique<SocketCanTransport>(interface_name);
}
}  // namespace rauch
