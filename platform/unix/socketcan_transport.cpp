#include "socketcan_transport.hpp"

#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace rauch
{
SocketCanTransport::SocketCanTransport(const std::string& interface_name)
{
    socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_ < 0)
        throw std::system_error(errno, std::generic_category(), "Cannot open SocketCAN socket");

    errno                      = 0;
    const auto interface_index = if_nametoindex(interface_name.c_str());
    if (interface_index == 0)
    {
        const auto error = errno == 0 ? ENODEV : errno;
        ::close(socket_);
        socket_ = -1;
        throw std::system_error(error, std::generic_category(), "Cannot find CAN interface " + interface_name);
    }

    sockaddr_can address{};
    address.can_family  = AF_CAN;
    address.can_ifindex = static_cast<int>(interface_index);
    if (::bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0)
    {
        const auto error = errno;
        ::close(socket_);
        socket_ = -1;
        throw std::system_error(error, std::generic_category(), "Cannot bind SocketCAN interface " + interface_name);
    }
}

SocketCanTransport::~SocketCanTransport()
{
    if (socket_ >= 0)
        ::close(socket_);
}

void SocketCanTransport::send(const CanFrame& frame)
{
    if (frame.length > CAN_MAX_DLEN)
        throw std::invalid_argument("CAN frame payload is larger than 8 bytes");
    if (frame.identifier > CAN_EFF_MASK)
        throw std::invalid_argument("CAN identifier is larger than 29 bits");

    can_frame message{};
    message.can_id  = frame.identifier;
    message.can_dlc = frame.length;
    if (frame.identifier > CAN_SFF_MASK)
        message.can_id |= CAN_EFF_FLAG;
    std::memcpy(message.data, frame.data.data(), frame.length);

    ssize_t count;
    do
    {
        count = ::write(socket_, &message, sizeof(message));
    } while (count < 0 && errno == EINTR);
    if (count != sizeof(message))
        throw std::system_error(count < 0 ? errno : EIO, std::generic_category(), "SocketCAN write failed");
}

CanFrame SocketCanTransport::receive()
{
    can_frame message{};
    ssize_t   count;
    do
    {
        count = ::read(socket_, &message, sizeof(message));
    } while (count < 0 && errno == EINTR);
    if (count != sizeof(message))
        throw std::system_error(count < 0 ? errno : EIO, std::generic_category(), "SocketCAN read failed");
    if (message.can_dlc > CAN_MAX_DLEN)
        throw std::runtime_error("SocketCAN returned a frame with invalid payload length");

    CanFrame frame{};
    frame.identifier = message.can_id & ((message.can_id & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK);
    frame.length     = message.can_dlc;
    std::memcpy(frame.data.data(), message.data, frame.length);
    return frame;
}
}  // namespace rauch
