#pragma once

#include "xf1_store.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace rauch
{
class TcpServer
{
public:
    TcpServer(std::uint16_t port, std::filesystem::path output_directory, std::size_t worker_count = 0,
              std::size_t max_pending_clients = 32, std::string listen_address = "152.21.0.31",
              std::unique_ptr<IXf1Store> store = nullptr);
    int run();
    ~TcpServer();
    void          stop() noexcept;
    std::uint16_t bound_port() const noexcept;

private:
    std::uint16_t              port_;
    std::filesystem::path      output_directory_;
    std::string                listen_address_;
    std::unique_ptr<IXf1Store> store_;
    std::size_t                worker_count_;
    std::size_t                max_pending_clients_;
    std::atomic<bool>          stopping_{ false };
    std::atomic<std::uint16_t> bound_port_{ 0 };
    std::mutex                 runtime_mutex_;
    class Runtime;
    std::unique_ptr<Runtime> runtime_;
};
}  // namespace rauch
