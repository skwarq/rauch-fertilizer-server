#include "wifi_interface.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(WifiInterfaceSelection, ChoosesFirstUsableWifiDevice)
{
    const auto selected = rauch::select_first_wifi_interface(
        "lo:loopback:connected "
        "(externally)\nenp2s0:ethernet:connected\nwlp0s20f3:wifi:connected\nwlan1:wifi:disconnected\n");
    EXPECT_EQ(selected, "wlp0s20f3");
}

TEST(WifiInterfaceSelection, SkipsUnavailableWifiDevice)
{
    const auto selected = rauch::select_first_wifi_interface("wlan0:wifi:unavailable\nwlan1:wifi:disconnected\n");
    EXPECT_EQ(selected, "wlan1");
}

TEST(WifiInterfaceSelection, FailsWhenNoUsableWifiDeviceExists)
{
    EXPECT_THROW(rauch::select_first_wifi_interface("lo:loopback:connected\nenp2s0:ethernet:connected\n"),
                 std::runtime_error);
}
