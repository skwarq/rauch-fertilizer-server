#include "can_transport.hpp"

#include <gtest/gtest.h>
#include <system_error>

TEST(SocketCanTransport, RejectsMissingInterfaceWithoutRequiringCanHardware)
{
    EXPECT_THROW(rauch::create_platform_can_transport("rauch-test-no-such-can-interface"), std::system_error);
}
