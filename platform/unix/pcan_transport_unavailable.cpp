#include "pcan_transport.hpp"

#include <stdexcept>

namespace rauch
{
PcanCanTransport::PcanCanTransport(std::uint16_t, std::uint16_t)
{
    throw std::runtime_error("PCAN transport is available only on Windows");
}
PcanCanTransport::~PcanCanTransport() = default;
void PcanCanTransport::send(const CanFrame&)
{
    throw std::runtime_error("PCAN transport is available only on Windows");
}
CanFrame PcanCanTransport::receive()
{
    throw std::runtime_error("PCAN transport is available only on Windows");
}
}  // namespace rauch
