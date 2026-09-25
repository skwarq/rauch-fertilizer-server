#include "pcan_transport.hpp"

#include <PCANBasic.h>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace rauch
{
namespace
{
    TPCANHandle handle(std::uint16_t channel)
    {
        return static_cast<TPCANHandle>(channel);
    }

    void check(TPCANStatus status, const char* operation)
    {
        if (status == PCAN_ERROR_OK)
            return;
        char message[256]{};
        CAN_GetErrorText(status, 0, message);
        throw std::runtime_error(std::string(operation) + " failed: " + message);
    }
}  // namespace

PcanCanTransport::PcanCanTransport(std::uint16_t channel, std::uint16_t bitrate) : channel_(channel)
{
    check(CAN_Initialize(handle(channel_), bitrate, 0, 0, 0), "CAN_Initialize");
    initialized_ = true;
}

PcanCanTransport::~PcanCanTransport()
{
    if (initialized_)
        CAN_Uninitialize(handle(channel_));
}

void PcanCanTransport::send(const CanFrame& frame)
{
    if (frame.length > frame.data.size())
        throw std::invalid_argument("CAN frame payload is larger than 8 bytes");
    if (frame.identifier > 0x1fffffffu)
        throw std::invalid_argument("CAN identifier is larger than 29 bits");
    TPCANMsg message{};
    message.ID      = frame.identifier;
    message.LEN     = frame.length;
    message.MSGTYPE = frame.identifier > 0x7ffu ? PCAN_MESSAGE_EXTENDED : PCAN_MESSAGE_STANDARD;
    std::copy_n(frame.data.begin(), frame.length, message.DATA);
    check(CAN_Write(handle(channel_), &message), "CAN_Write");
}

CanFrame PcanCanTransport::receive()
{
    TPCANMsg       message{};
    TPCANTimestamp timestamp{};
    check(CAN_Read(handle(channel_), &message, &timestamp), "CAN_Read");
    if ((message.MSGTYPE & PCAN_MESSAGE_ERRFRAME) != 0)
        throw std::runtime_error("PCAN returned a CAN error frame");
    if ((message.MSGTYPE & PCAN_MESSAGE_RTR) != 0)
        throw std::runtime_error("PCAN returned a remote frame; data frames are required");
    if (message.LEN > 8)
        throw std::runtime_error("PCAN returned a frame with invalid payload length");
    CanFrame frame{};
    frame.identifier = message.ID;
    frame.length     = message.LEN;
    std::copy_n(message.DATA, frame.length, frame.data.begin());
    return frame;
}
}  // namespace rauch
