#pragma once

#include <memory>
#include <string>

namespace rauch
{
struct HotspotConfig
{
    std::string ssid     = "quantron";
    std::string password = "quantron";
    std::string address  = "152.21.0.31/24";
};

class IHotspot
{
public:
    virtual ~IHotspot()          = default;
    virtual void start()         = 0;
    virtual void stop() noexcept = 0;
};

std::unique_ptr<IHotspot> create_platform_hotspot(HotspotConfig config);
}  // namespace rauch
