#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace rauch
{
struct CanFrame
{
    std::uint32_t               identifier = 0;
    std::array<std::uint8_t, 8> data{};
    std::uint8_t                length = 0;
};

class ICanTransport
{
public:
    virtual ~ICanTransport()                     = default;
    virtual void     send(const CanFrame& frame) = 0;
    virtual CanFrame receive()                   = 0;
};

class UnsupportedCanTransport final : public ICanTransport
{
public:
    explicit UnsupportedCanTransport(std::string backend_name);
    void     send(const CanFrame& frame) override;
    CanFrame receive() override;

private:
    std::string backend_name_;
};

std::unique_ptr<ICanTransport> create_platform_can_transport(std::string interface_name = "can0");
}  // namespace rauch
