#include "can_transport.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(UnsupportedCanTransport, ReportsBackendReason)
{
    rauch::UnsupportedCanTransport backend("test backend");
    rauch::CanFrame                frame{};
    EXPECT_THROW(backend.send(frame), std::runtime_error);
    EXPECT_THROW(backend.receive(), std::runtime_error);
}
