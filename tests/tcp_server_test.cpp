#include "boost_process.hpp"
#include "tcp_server.hpp"

#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <future>
#include <gtest/gtest.h>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>

#if !defined(_WIN32)
#    include <unistd.h>
#endif

namespace
{
using namespace std::chrono_literals;

class TemporaryDirectory final
{
public:
    explicit TemporaryDirectory(const std::string& prefix)
        : path_(std::filesystem::temp_directory_path()
                / (prefix + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

class BlockingStore final : public rauch::IXf1Store
{
public:
    std::filesystem::path store(const rauch::Xf1Frame&, const rauch::Xf1Record&) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        entered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this] { return released_; });
        completed_ = true;
        condition_.notify_all();
        return "test.xf1";
    }

    bool wait_until_entered()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, 2s, [this] { return entered_; });
    }

    void release()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        released_ = true;
        condition_.notify_all();
    }

    bool completed()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return completed_;
    }

private:
    std::mutex              mutex_;
    std::condition_variable condition_;
    bool                    entered_   = false;
    bool                    released_  = false;
    bool                    completed_ = false;
};

rauch::Xf1Frame valid_frame()
{
    rauch::Xf1Frame frame{};
    frame[0]                 = 206;
    frame[8]                 = 3;
    frame[12]                = 1;
    constexpr char product[] = "TCP Fertilizer";
    std::copy(std::begin(product), std::end(product) - 1, frame.begin() + 114);
    std::uint32_t checksum = 0;
    for (std::size_t index = 16; index < frame.size(); ++index)
        checksum += frame[index];
    frame[4] = static_cast<std::uint8_t>(checksum);
    frame[5] = static_cast<std::uint8_t>(checksum >> 8);
    return frame;
}

TEST(TcpServer, StopTerminatesAcceptLoopAndClosesIncompleteClient)
{
    TemporaryDirectory output("rauch-tcp-stop-test-");
    rauch::TcpServer   server(0, output.path(), 2, 4, "127.0.0.1");
    auto               running = std::async(std::launch::async, [&server] { return server.run(); });

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (server.bound_port() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(5ms);
    ASSERT_NE(server.bound_port(), 0);

    boost::asio::io_context      context;
    boost::asio::ip::tcp::socket client(context);
    client.connect({ boost::asio::ip::address_v4::loopback(), server.bound_port() });

    server.stop();
    ASSERT_EQ(running.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(running.get(), 0);
}

TEST(TcpServer, SigintStopsAcceptLoop)
{
#if defined(_WIN32)
    GTEST_SKIP() << "This integration test uses POSIX process-directed SIGINT";
#else
    rauch::process::ipstream output;
    rauch::process::child    child(RAUCH_TEST_TCP_SIGNAL_PROCESS_PATH, rauch::process::std_out > output);
    std::string              readiness;
    bool                     ready = false;
    while (std::getline(output, readiness))
    {
        if (readiness == "READY")
        {
            ready = true;
            break;
        }
    }
    const int signal_result = ready ? ::kill(static_cast<pid_t>(child.id()), SIGINT) : -1;
    child.wait();
    EXPECT_TRUE(ready);
    EXPECT_EQ(signal_result, 0);
    EXPECT_EQ(child.exit_code(), 0);
#endif
}

TEST(TcpServer, ReceivesValidXf1AndStoresExactFrame)
{
    TemporaryDirectory output("rauch-tcp-upload-test-");
    rauch::TcpServer   server(0, output.path(), 2, 4, "127.0.0.1");
    auto               running  = std::async(std::launch::async, [&server] { return server.run(); });
    const auto         deadline = std::chrono::steady_clock::now() + 2s;
    while (server.bound_port() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(5ms);

    bool sent = false;
    if (server.bound_port() != 0)
    {
        boost::asio::io_context      context;
        boost::asio::ip::tcp::socket client(context);
        boost::system::error_code    error;
        client.connect({ boost::asio::ip::address_v4::loopback(), server.bound_port() }, error);
        if (!error)
        {
            const auto frame = valid_frame();
            boost::asio::write(client, boost::asio::buffer(frame), error);
            sent = !error;
        }
    }

    const auto stored_file     = output.path() / "TCP_Fertilizer.xf1";
    const auto stored_deadline = std::chrono::steady_clock::now() + 2s;
    while (sent && !std::filesystem::exists(stored_file) && std::chrono::steady_clock::now() < stored_deadline)
        std::this_thread::sleep_for(5ms);
    const bool                                stored = std::filesystem::exists(stored_file);
    std::array<std::uint8_t, rauch::xf1_size> contents{};
    if (stored)
    {
        std::ifstream input(stored_file, std::ios::binary);
        input.read(reinterpret_cast<char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
    }

    server.stop();
    ASSERT_EQ(running.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(running.get(), 0);
    EXPECT_TRUE(sent);
    ASSERT_TRUE(stored);
    EXPECT_EQ(contents, valid_frame());
}

TEST(TcpServer, StopDrainsAnUploadAlreadyBeingStored)
{
    TemporaryDirectory output("rauch-tcp-drain-test-");
    auto               store          = std::make_unique<BlockingStore>();
    auto*              store_observer = store.get();
    rauch::TcpServer   server(0, output.path(), 1, 4, "127.0.0.1", std::move(store));
    auto               running = std::async(std::launch::async, [&server] { return server.run(); });

    const auto listen_deadline = std::chrono::steady_clock::now() + 2s;
    while (server.bound_port() == 0 && std::chrono::steady_clock::now() < listen_deadline)
        std::this_thread::sleep_for(5ms);
    ASSERT_NE(server.bound_port(), 0);

    boost::asio::io_context      context;
    boost::asio::ip::tcp::socket client(context);
    client.connect({ boost::asio::ip::address_v4::loopback(), server.bound_port() });
    const auto frame = valid_frame();
    boost::asio::write(client, boost::asio::buffer(frame));
    ASSERT_TRUE(store_observer->wait_until_entered());

    server.stop();
    EXPECT_EQ(running.wait_for(20ms), std::future_status::timeout);
    store_observer->release();
    ASSERT_EQ(running.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(running.get(), 0);
    EXPECT_TRUE(store_observer->completed());
}
}  // namespace
