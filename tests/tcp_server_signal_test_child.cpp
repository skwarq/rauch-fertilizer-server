#include "tcp_server.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main()
{
    rauch::TcpServer server(0, std::filesystem::temp_directory_path() / "rauch-signal-child", 2, 4, "127.0.0.1");
    int              result = -1;
    std::thread      worker([&] { result = server.run(); });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.bound_port() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (server.bound_port() == 0)
    {
        server.stop();
        worker.join();
        return 1;
    }

    std::cout << "READY" << std::endl;
    worker.join();
    return result;
}
