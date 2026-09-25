#include "boost_process.hpp"
#include "cli_options.hpp"
#include "hotspot.hpp"
#include "isobusfs_process.hpp"
#include "tcp_server.hpp"

#include <boost/asio/ip/address_v4.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void print_usage(const char* executable)
{
    std::cout << "Usage: " << executable
              << " [options]\n\nOptions:\n"
                 "  --port PORT                  TCP port (default: 8172)\n"
                 "  --output-dir PATH            XF1 output directory (default: $TEMP/rauch-fs)\n"
                 "  --can-interface NAME         CAN interface (default: vcan0)\n"
                 "  --j1939-address ADDRESS      J1939 address\n"
                 "  --isobusfs-executable PATH   ISO11783-13 server executable\n"
                 "  --listen-address IPv4        TCP listen address (default: 152.21.0.31)\n"
                 "  --no-hotspot                 Do not start the Wi-Fi hotspot\n"
                 "  --no-isobusfs                Do not start the ISO11783-13 server\n"
                 "  --help                       Show this help\n\n"
                 "Positional arguments are deprecated: PORT OUTPUT_DIR CAN_INTERFACE J1939_ADDRESS EXECUTABLE."
              << std::endl;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--help")
    {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }
    try
    {
        std::vector<std::string>   positional;
        bool                       start_hotspot                = true;
        bool                       start_isobusfs               = true;
        bool                       show_help                    = false;
        bool                       explicit_isobusfs_executable = false;
        std::string                listen_address               = "152.21.0.31";
        std::optional<std::string> named_port;
        std::optional<std::string> named_output;
        std::optional<std::string> named_interface;
        std::optional<std::string> named_address;
        rauch::IsobusFsConfig      isobus_config;
        for (int index = 1; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--help")
                show_help = true;
            else if (option == "--no-hotspot")
                start_hotspot = false;
            else if (option == "--no-isobusfs")
                start_isobusfs = false;
            else if (option == "--port" || option == "--output-dir" || option == "--can-interface"
                     || option == "--j1939-address" || option == "--listen-address"
                     || option == "--isobusfs-executable")
            {
                if (++index >= argc)
                    throw std::invalid_argument("Missing value for " + option);
                if (option == "--port")
                    named_port = argv[index];
                else if (option == "--output-dir")
                    named_output = argv[index];
                else if (option == "--can-interface")
                    named_interface = argv[index];
                else if (option == "--j1939-address")
                    named_address = argv[index];
                else if (option == "--listen-address")
                    listen_address = argv[index];
                else
                {
                    isobus_config.executable     = argv[index];
                    explicit_isobusfs_executable = true;
                }
            }
            else if (option.rfind("--", 0) == 0)
                throw std::invalid_argument("Unknown option: " + option);
            else
                positional.push_back(option);
        }
        if (show_help)
        {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (positional.size() > 5)
            throw std::invalid_argument("Too many positional arguments");
        if (explicit_isobusfs_executable && positional.size() > 4)
            throw std::invalid_argument("Specify the ISO11783-13 server executable only once");
        if (!start_isobusfs && explicit_isobusfs_executable)
            throw std::invalid_argument("--isobusfs-executable cannot be combined with --no-isobusfs");
        if (named_port && !positional.empty())
            throw std::invalid_argument("--port cannot be combined with positional arguments");

        const auto port =
            named_port ? rauch::parse_tcp_port(*named_port) :
                         (positional.empty() ? static_cast<std::uint16_t>(8172) : rauch::parse_tcp_port(positional[0]));
        const std::filesystem::path output =
            named_output ? std::filesystem::path(*named_output) :
                           (positional.size() > 1 ? std::filesystem::path(positional[1]) :
                                                    std::filesystem::temp_directory_path() / "rauch-fs");
        isobus_config.directory = output;
        std::filesystem::path executable_path(argv[0]);
        if (!executable_path.has_parent_path())
        {
            const auto located = rauch::process::search_path(executable_path.string());
            if (!located.empty())
                executable_path = located.string();
        }
        const auto sibling_server = std::filesystem::absolute(executable_path).parent_path() / "isobusfs-srv";
        if (positional.size() > 4)
            isobus_config.executable = positional[4];
        else if (!explicit_isobusfs_executable && std::filesystem::exists(sibling_server))
            isobus_config.executable = sibling_server;
        else if (!explicit_isobusfs_executable && std::filesystem::exists(sibling_server.string() + ".exe"))
            isobus_config.executable = sibling_server.string() + ".exe";
        if (named_interface)
            isobus_config.interface_name = *named_interface;
        else if (positional.size() > 2)
            isobus_config.interface_name = positional[2];
        if (named_address)
            isobus_config.address = *named_address;
        else if (positional.size() > 3)
            isobus_config.address = positional[3];
        static_cast<void>(boost::asio::ip::make_address_v4(listen_address));

        std::unique_ptr<rauch::IHotspot> hotspot;
        if (start_hotspot)
        {
            hotspot = rauch::create_platform_hotspot({});
            hotspot->start();
        }

        std::unique_ptr<rauch::IsobusFsProcess> isobus_server;
        if (start_isobusfs)
        {
            isobus_server = std::make_unique<rauch::IsobusFsProcess>(isobus_config);
            isobus_server->start();
        }
        return rauch::TcpServer(port, output, 0, 32, listen_address).run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
