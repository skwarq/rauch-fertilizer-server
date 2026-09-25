#include "hotspot.hpp"

#include <stdexcept>

namespace rauch
{
std::unique_ptr<IHotspot> create_platform_hotspot(HotspotConfig)
{
    throw std::runtime_error("Hotspot management is currently implemented only for Linux with NetworkManager");
}
}  // namespace rauch
