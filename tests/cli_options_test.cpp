#include "cli_options.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(CommandLineOptions, ParsesValidTcpPorts)
{
    EXPECT_EQ(rauch::parse_tcp_port("8172"), 8172);
    EXPECT_EQ(rauch::parse_tcp_port("0"), 0);
    EXPECT_EQ(rauch::parse_tcp_port("65535"), 65535);
}

TEST(CommandLineOptions, RejectsInvalidTcpPorts)
{
    EXPECT_THROW(rauch::parse_tcp_port(""), std::invalid_argument);
    EXPECT_THROW(rauch::parse_tcp_port("-1"), std::invalid_argument);
    EXPECT_THROW(rauch::parse_tcp_port("65536"), std::invalid_argument);
    EXPECT_THROW(rauch::parse_tcp_port("8172oops"), std::invalid_argument);
}
