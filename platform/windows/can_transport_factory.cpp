#include "can_transport.hpp"

#include <memory>

namespace rauch
{
std::unique_ptr<ICanTransport> create_platform_can_transport(std::string interface_name)
{
    return std::make_unique<UnsupportedCanTransport>("Windows PCAN backend is not enabled for " + interface_name);
}
}  // namespace rauch
