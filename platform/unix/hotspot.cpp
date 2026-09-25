#include "hotspot.hpp"

#include "boost_process.hpp"
#include "wifi_interface.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace rauch
{
namespace
{
    class NetworkManagerHotspot final : public IHotspot
    {
    public:
        explicit NetworkManagerHotspot(HotspotConfig config) : config_(std::move(config))
        {
            if (config_.password.size() < 8 || config_.password.size() > 63)
                throw std::invalid_argument("WPA2 hotspot password must contain 8 to 63 characters");
            nmcli_ = process::search_path("nmcli");
            if (nmcli_.empty())
                throw std::runtime_error("NetworkManager nmcli was not found in PATH");
        }

        ~NetworkManagerHotspot() override { stop(); }

        void start() override
        {
            if (started_)
                throw std::logic_error("Hotspot is already running");
            interface_name_ = first_wifi_interface();
            std::cout << "Selected first available Wi-Fi interface: " << interface_name_ << std::endl;
            previous_uuid_ = active_connection_uuid();
            profile_name_  = "rauch-hotspot-" + std::to_string(std::random_device{}());
            run({ "connection", "add", "type", "wifi", "ifname", interface_name_, "con-name", profile_name_, "ssid",
                  config_.ssid });
            profile_created_ = true;
            try
            {
                run({ "connection", "modify", profile_name_, "802-11-wireless.mode", "ap", "802-11-wireless.band", "bg",
                      "wifi-sec.key-mgmt", "wpa-psk", "wifi-sec.psk", config_.password, "ipv4.method", "shared",
                      "ipv4.addresses", config_.address, "connection.autoconnect", "no" });
                run({ "connection", "up", profile_name_, "ifname", interface_name_ });
                started_ = true;
                std::cout << "Hotspot SSID=" << config_.ssid << " interface=" << interface_name_
                          << " address=" << config_.address << std::endl;
            }
            catch (...)
            {
                cleanup();
                throw;
            }
        }

        void stop() noexcept override
        {
            if (!profile_created_)
                return;
            cleanup();
        }

    private:
        std::string first_wifi_interface() const
        {
            process::ipstream child_output;
            process::child    child(nmcli_, "-t", "-f", "DEVICE,TYPE,STATE", "device", "status",
                                    process::std_out > child_output);
            std::string       status;
            std::thread       reader(
                [&child_output, &status]
                {
                    std::string line;
                    while (std::getline(child_output, line))
                        status += line + "\n";
                });
            try
            {
                wait_for_process(child, "enumerating NetworkManager devices");
            }
            catch (...)
            {
                terminate_and_reap(child);
                reader.join();
                throw;
            }
            reader.join();
            if (child.exit_code() != 0)
                throw std::runtime_error("Could not enumerate NetworkManager devices");
            return select_first_wifi_interface(status);
        }

        void run(const std::vector<std::string>& args) const
        {
            process::child child(nmcli_, args);
            try
            {
                wait_for_process(child, "running nmcli");
            }
            catch (...)
            {
                terminate_and_reap(child);
                throw;
            }
            if (child.exit_code() != 0)
                throw std::runtime_error("nmcli command failed (exit " + std::to_string(child.exit_code()) + ")");
        }

        std::string active_connection_uuid() const
        {
            process::ipstream child_output;
            process::child    child(nmcli_, "-g", "GENERAL.CON-UUID", "device", "show", interface_name_,
                                    process::std_out > child_output);
            std::string       uuid;
            std::thread       reader([&child_output, &uuid] { std::getline(child_output, uuid); });
            try
            {
                wait_for_process(child, "reading the active Wi-Fi connection");
            }
            catch (...)
            {
                terminate_and_reap(child);
                reader.join();
                throw;
            }
            reader.join();
            if (child.exit_code() != 0 || uuid == "--")
                return {};
            return uuid;
        }

        static void wait_for_process(process::child& child, const char* operation)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (std::chrono::steady_clock::now() < deadline)
            {
                std::error_code error;
                if (!child.running(error))
                {
                    if (error)
                        throw std::system_error(error, "Could not inspect nmcli process");
                    child.wait();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            throw std::runtime_error(std::string("Timed out after 8 seconds while ") + operation);
        }

        static void terminate_and_reap(process::child& child) noexcept
        {
            std::error_code terminate_error;
            child.terminate(terminate_error);
            std::error_code wait_error;
            child.wait(wait_error);
            if (terminate_error)
                std::cerr << "Could not terminate nmcli process: " << terminate_error.message() << std::endl;
            if (wait_error)
                std::cerr << "Could not reap nmcli process: " << wait_error.message() << std::endl;
        }

        void cleanup() noexcept
        {
            const auto cleanup_step = [this](const std::vector<std::string>& args)
            {
                try
                {
                    run(args);
                }
                catch (const std::exception& error)
                {
                    std::cerr << "Hotspot cleanup failed: " << error.what() << std::endl;
                }
            };
            if (started_)
                cleanup_step({ "connection", "down", profile_name_ });
            if (profile_created_)
                cleanup_step({ "connection", "delete", profile_name_ });
            profile_created_ = false;
            started_         = false;
            if (!previous_uuid_.empty())
                cleanup_step({ "connection", "up", "uuid", previous_uuid_, "ifname", interface_name_ });
        }

        HotspotConfig                           config_;
        decltype(process::search_path("nmcli")) nmcli_;
        std::string                             interface_name_;
        std::string                             profile_name_;
        std::string                             previous_uuid_;
        bool                                    profile_created_ = false;
        bool                                    started_         = false;
    };
}  // namespace

std::unique_ptr<IHotspot> create_platform_hotspot(HotspotConfig config)
{
    return std::make_unique<NetworkManagerHotspot>(std::move(config));
}
}  // namespace rauch
