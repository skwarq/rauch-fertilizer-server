#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace rauch
{
struct IsobusFsConfig
{
    std::filesystem::path executable     = "isobusfs-srv";
    std::string           interface_name = "vcan0";
    std::string           address        = "f8";
    std::string           volume         = "vol1";
    std::filesystem::path directory      = std::filesystem::temp_directory_path() / "rauch-fs";
    int                   log_level      = 4;
};

class IsobusFsProcess
{
public:
    explicit IsobusFsProcess(IsobusFsConfig config);
    ~IsobusFsProcess();
    IsobusFsProcess(const IsobusFsProcess&)            = delete;
    IsobusFsProcess& operator=(const IsobusFsProcess&) = delete;
    void             start();
    void             stop() noexcept;
    bool             is_running() const noexcept;

private:
    struct State;
    IsobusFsConfig         config_;
    std::unique_ptr<State> state_;
};
}  // namespace rauch
