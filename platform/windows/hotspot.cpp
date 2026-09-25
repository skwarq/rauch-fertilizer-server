#include "hotspot.hpp"

// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <wlanapi.h>
#include "hosted_network_api_compat.hpp"
#include <iphlpapi.h>
#include <netioapi.h>
#include <ws2tcpip.h>
// clang-format on

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rauch
{
namespace
{
    class WlanMemory final
    {
    public:
        ~WlanMemory()
        {
            if (value_)
                WlanFreeMemory(value_);
        }

        PVOID* out() noexcept { return &value_; }
        PVOID  get() const noexcept { return value_; }

    private:
        PVOID value_ = nullptr;
    };

    std::runtime_error wlan_error(const char* operation, DWORD error)
    {
        return std::runtime_error(std::string(operation) + " failed with Windows error " + std::to_string(error));
    }

    class HostedNetworkHotspot final : public IHotspot
    {
    public:
        explicit HostedNetworkHotspot(HotspotConfig config) : config_(std::move(config))
        {
            if (config_.password.size() < 8 || config_.password.size() > 63)
                throw std::invalid_argument("Hosted Network passphrase must contain 8 to 63 characters");
            if (config_.ssid.empty() || config_.ssid.size() > DOT11_SSID_MAX_LENGTH)
                throw std::invalid_argument("Hosted Network SSID must contain 1 to 32 bytes");
            if (config_.address != "152.21.0.31/24")
                throw std::invalid_argument("Windows Hosted Network currently requires address 152.21.0.31/24");

            DWORD       negotiated_version = 0;
            const DWORD result             = WlanOpenHandle(2, nullptr, &negotiated_version, &client_);
            if (result != ERROR_SUCCESS)
                throw wlan_error("WlanOpenHandle", result);
        }

        ~HostedNetworkHotspot() override
        {
            stop();
            if (client_)
                WlanCloseHandle(client_, nullptr);
        }

        void start() override
        {
            if (started_)
                throw std::logic_error("Hosted Network is already running");

            try
            {
                ensure_inactive_and_save_configuration();
                settings_changed_ = true;
                set_ssid(config_.ssid);
                set_secondary_key(config_.password, TRUE);

                WLAN_HOSTED_NETWORK_REASON reason       = wlan_hosted_network_reason_success;
                const DWORD                start_result = WlanHostedNetworkStartUsing(client_, &reason, nullptr);
                if (start_result != ERROR_SUCCESS)
                    throw wlan_error("WlanHostedNetworkStartUsing (check driver support and WLAN AutoConfig)",
                                     start_result);
                started_ = true;

                add_rauch_address();
                std::cout << "Windows Hosted Network started: SSID=" << config_.ssid << " address=" << config_.address
                          << " (Windows standalone ICS supplies client DHCP)" << std::endl;
            }
            catch (...)
            {
                stop();
                throw;
            }
        }

        void stop() noexcept override
        {
            if (address_context_ != 0)
            {
                const DWORD result = DeleteIPAddress(address_context_);
                if (result != NO_ERROR)
                    std::cerr << "DeleteIPAddress failed with Windows error " << result << std::endl;
                address_context_ = 0;
            }

            if (started_)
            {
                WLAN_HOSTED_NETWORK_REASON reason = wlan_hosted_network_reason_success;
                const DWORD                result = WlanHostedNetworkStopUsing(client_, &reason, nullptr);
                if (result != ERROR_SUCCESS)
                    std::cerr << "WlanHostedNetworkStopUsing failed with Windows error " << result << std::endl;
                started_ = false;
            }
            restore_settings();
        }

    private:
        void ensure_inactive_and_save_configuration()
        {
            PWLAN_HOSTED_NETWORK_STATUS status        = nullptr;
            const DWORD                 status_result = WlanHostedNetworkQueryStatus(client_, &status, nullptr);
            if (status_result == ERROR_SUCCESS && status)
            {
                WlanFreeMemory(status);
            }

            query_connection_settings();
            if (!had_previous_settings_)
            {
                WLAN_HOSTED_NETWORK_REASON reason      = wlan_hosted_network_reason_success;
                const DWORD                init_result = WlanHostedNetworkInitSettings(client_, &reason, nullptr);
                if (init_result != ERROR_SUCCESS)
                    throw wlan_error("WlanHostedNetworkInitSettings (driver may not support Hosted Network)",
                                     init_result);
                query_connection_settings();
                if (!had_previous_settings_)
                    throw std::runtime_error("Could not read initialized Hosted Network settings to roll them back");
                settings_created_by_app_ = true;
            }
        }

        void query_connection_settings()
        {
            DWORD                    size   = 0;
            rauch_wlan_property_type opcode = {};
            WlanMemory               memory;
            const DWORD result = WlanHostedNetworkQueryProperty(client_, wlan_hosted_network_opcode_connection_settings,
                                                                &size, memory.out(), &opcode, nullptr);
            if (result == ERROR_SUCCESS && memory.get() && size >= sizeof(WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS))
            {
                previous_settings_     = *static_cast<const WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS*>(memory.get());
                had_previous_settings_ = true;
                return;
            }
            if (result != ERROR_SUCCESS && result != ERROR_INVALID_STATE && result != ERROR_NOT_FOUND)
                throw wlan_error("WlanHostedNetworkQueryProperty(connection settings)", result);
            had_previous_settings_ = false;
        }

        void set_ssid(const std::string& ssid)
        {
            WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS settings{};
            if (had_previous_settings_)
                settings = previous_settings_;
            settings.hostedNetworkSSID.uSSIDLength = static_cast<ULONG>(ssid.size());
            std::memcpy(settings.hostedNetworkSSID.ucSSID, ssid.data(), ssid.size());
            if (settings.dwMaxNumberOfPeers == 0)
                settings.dwMaxNumberOfPeers = 8;

            WLAN_HOSTED_NETWORK_REASON reason = wlan_hosted_network_reason_success;
            const DWORD result = WlanHostedNetworkSetProperty(client_, wlan_hosted_network_opcode_connection_settings,
                                                              sizeof(settings), &settings, &reason, nullptr);
            if (result != ERROR_SUCCESS)
                throw wlan_error("WlanHostedNetworkSetProperty(connection settings)", result);
        }

        void restore_settings() noexcept
        {
            if (settings_changed_ && had_previous_settings_)
            {
                WLAN_HOSTED_NETWORK_REASON reason = wlan_hosted_network_reason_success;
                const DWORD                result =
                    WlanHostedNetworkSetProperty(client_, wlan_hosted_network_opcode_connection_settings,
                                                 sizeof(previous_settings_), &previous_settings_, &reason, nullptr);
                if (result != ERROR_SUCCESS)
                    std::cerr << "Could not restore previous Hosted Network settings (Windows error " << result << ")"
                              << std::endl;
            }
            settings_changed_ = false;
            if (settings_created_by_app_)
                std::cerr << "Windows WLAN API cannot delete the Hosted Network configuration created during startup\n";
            settings_created_by_app_ = false;
        }

        void set_secondary_key(const std::string& key, BOOL is_permanent)
        {
            WLAN_HOSTED_NETWORK_REASON reason = wlan_hosted_network_reason_success;
            const DWORD                result =
#if defined(__MINGW32__)
                WlanHostedNetworkSetSecondaryKey(client_, static_cast<DWORD>(key.size() + 1),
                                                 reinterpret_cast<PUCHAR>(const_cast<char*>(key.c_str())), is_permanent,
                                                 FALSE, &reason, nullptr);
#else
                WlanHostedNetworkSetSecondaryKey(client_, static_cast<DWORD>(key.size() + 1),
                                                 reinterpret_cast<PUCHAR>(const_cast<char*>(key.c_str())), is_permanent,
                                                 TRUE, &reason, nullptr);
#endif
            if (result != ERROR_SUCCESS)
                throw wlan_error("WlanHostedNetworkSetSecondaryKey", result);
        }

        void add_rauch_address()
        {
            PWLAN_HOSTED_NETWORK_STATUS status       = nullptr;
            const DWORD                 query_result = WlanHostedNetworkQueryStatus(client_, &status, nullptr);
            if (query_result != ERROR_SUCCESS || !status)
                throw wlan_error("WlanHostedNetworkQueryStatus", query_result);
            const GUID adapter_guid = status->IPDeviceID;
            WlanFreeMemory(status);

            NET_LUID           luid{};
            const NETIO_STATUS luid_result = ConvertInterfaceGuidToLuid(&adapter_guid, &luid);
            if (luid_result != NO_ERROR)
                throw wlan_error("ConvertInterfaceGuidToLuid", luid_result);
            NET_IFINDEX        interface_index = 0;
            const NETIO_STATUS index_result    = ConvertInterfaceLuidToIndex(&luid, &interface_index);
            if (index_result != NO_ERROR)
                throw wlan_error("ConvertInterfaceLuidToIndex", index_result);

            IN_ADDR address_value{};
            if (InetPtonA(AF_INET, "152.21.0.31", &address_value) != 1)
                throw std::runtime_error("Could not parse Hosted Network IPv4 address");
            const ULONG address = address_value.S_un.S_addr;
            if (address_exists(address, interface_index))
            {
                std::cout << "RAUCH address 152.21.0.31 is already configured on the Hosted Network adapter"
                          << std::endl;
                return;
            }

            IN_ADDR mask_value{};
            if (InetPtonA(AF_INET, "255.255.255.0", &mask_value) != 1)
                throw std::runtime_error("Could not parse Hosted Network IPv4 mask");
            const ULONG mask             = mask_value.S_un.S_addr;
            ULONG       address_instance = 0;
            const DWORD add_result = AddIPAddress(address, mask, interface_index, &address_context_, &address_instance);
            if (add_result != NO_ERROR)
                throw wlan_error("AddIPAddress(152.21.0.31/24); run elevated and check for address conflicts",
                                 add_result);
        }

        bool address_exists(ULONG address, NET_IFINDEX hosted_interface_index) const
        {
            ULONG                      buffer_size = 16 * 1024;
            std::vector<unsigned char> buffer(buffer_size);
            auto*                      adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
            ULONG                      result   = GetAdaptersAddresses(
                AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters,
                &buffer_size);
            if (result == ERROR_BUFFER_OVERFLOW)
            {
                buffer.resize(buffer_size);
                adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
                result   = GetAdaptersAddresses(
                    AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr,
                    adapters, &buffer_size);
            }
            if (result != NO_ERROR)
                throw wlan_error("GetAdaptersAddresses", result);

            for (auto* adapter = adapters; adapter; adapter = adapter->Next)
            {
                for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next)
                {
                    if (unicast->Address.lpSockaddr->sa_family != AF_INET)
                        continue;
                    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(unicast->Address.lpSockaddr);
                    if (ipv4->sin_addr.s_addr != address)
                        continue;
                    if (adapter->IfIndex != hosted_interface_index)
                        throw std::runtime_error("RAUCH address 152.21.0.31 is already assigned to another adapter");
                    return true;
                }
            }
            return false;
        }

        HotspotConfig                           config_;
        HANDLE                                  client_ = nullptr;
        WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS previous_settings_{};
        ULONG                                   address_context_         = 0;
        bool                                    had_previous_settings_   = false;
        bool                                    settings_changed_        = false;
        bool                                    settings_created_by_app_ = false;
        bool                                    started_                 = false;
    };
}  // namespace

std::unique_ptr<IHotspot> create_platform_hotspot(HotspotConfig config)
{
    return std::make_unique<HostedNetworkHotspot>(std::move(config));
}
}  // namespace rauch
