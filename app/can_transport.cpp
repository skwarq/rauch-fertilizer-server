#include "can_transport.hpp"

#include <stdexcept>
#include <utility>

namespace rauch
{
UnsupportedCanTransport::UnsupportedCanTransport(std::string backend_name) : backend_name_(std::move(backend_name))
{
}

void UnsupportedCanTransport::send(const CanFrame&)
{
    throw std::runtime_error("CAN backend is not configured: " + backend_name_);
}

CanFrame UnsupportedCanTransport::receive()
{
    throw std::runtime_error("CAN backend is not configured: " + backend_name_);
}
}  // namespace rauch
