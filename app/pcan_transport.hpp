#pragma once

#include "can_transport.hpp"

#include <cstdint>

namespace rauch
{
class PcanCanTransport final : public ICanTransport
{
public:
    PcanCanTransport(std::uint16_t channel, std::uint16_t bitrate);
    ~PcanCanTransport() override;
    PcanCanTransport(const PcanCanTransport&)            = delete;
    PcanCanTransport& operator=(const PcanCanTransport&) = delete;

    void     send(const CanFrame& frame) override;
    CanFrame receive() override;

private:
#ifdef _WIN32
    std::uint16_t channel_;
    bool          initialized_ = false;
#endif
};
}  // namespace rauch
