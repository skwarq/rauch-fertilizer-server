#pragma once

#include "can_transport.hpp"

#include <string>

namespace rauch
{
class SocketCanTransport final : public ICanTransport
{
public:
    explicit SocketCanTransport(const std::string& interface_name);
    ~SocketCanTransport() override;
    SocketCanTransport(const SocketCanTransport&)            = delete;
    SocketCanTransport& operator=(const SocketCanTransport&) = delete;

    void     send(const CanFrame& frame) override;
    CanFrame receive() override;

private:
    int socket_ = -1;
};
}  // namespace rauch
