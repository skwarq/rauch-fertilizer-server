#include "isobusfs_process.hpp"

#include "process_launcher.hpp"
#include "process_shutdown.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace rauch
{
struct IsobusFsProcess::State
{
    std::unique_ptr<process::child> child;
};

IsobusFsProcess::IsobusFsProcess(IsobusFsConfig config) : config_(std::move(config)), state_(std::make_unique<State>())
{
}

IsobusFsProcess::~IsobusFsProcess()
{
    stop();
}

void IsobusFsProcess::start()
{
    if (state_->child)
        throw std::logic_error("isobusfs-srv is already running");
    auto executable = config_.executable;
    if (std::filesystem::exists(executable))
    {
        executable = std::filesystem::absolute(executable);
    }
    else if (!executable.has_parent_path())
    {
        const auto located = process::search_path(executable.string());
        if (located.empty())
            throw std::runtime_error("isobusfs-srv was not found. can-utils ISO11783-13 is available only on Linux; "
                                     "provide a compatible executable or start with --no-isobusfs.");
        executable = located.string();
    }
    else
    {
        throw std::runtime_error("isobusfs-srv executable does not exist: " + executable.string());
    }
    std::filesystem::create_directories(config_.directory);
    const std::vector<std::string> arguments = { "-i", config_.interface_name,
                                                 "-a", config_.address,
                                                 "-v", config_.volume + ":" + config_.directory.string(),
                                                 "-w", config_.volume,
                                                 "-l", std::to_string(config_.log_level) };
    state_->child                            = launch_isobusfs_process(executable, arguments);
    if (!state_->child->running())
        throw std::runtime_error("failed to start isobusfs-srv");
    std::cout << "Started ISO11783-13 File Server: " << executable << '\n';
}

void IsobusFsProcess::stop() noexcept
{
    if (!state_ || !state_->child)
        return;

    auto&           child = *state_->child;
    std::error_code process_error;
    if (child.running(process_error) && !process_error)
    {
        if (!request_process_shutdown(static_cast<std::uint64_t>(child.id())))
            std::cerr << "Could not request graceful shutdown for isobusfs-srv; forcing termination\n";

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool       exited   = false;
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::error_code wait_error;
            if (!child.running(wait_error))
            {
                exited = !wait_error;
                if (wait_error)
                    std::cerr << "Could not wait for isobusfs-srv: " << wait_error.message() << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!exited)
        {
            std::cerr << "isobusfs-srv did not stop within 2 seconds; forcing termination\n";
            std::error_code terminate_error;
            child.terminate(terminate_error);
            if (terminate_error)
                std::cerr << "Could not terminate isobusfs-srv: " << terminate_error.message() << std::endl;
        }
    }
    else if (process_error)
    {
        std::cerr << "Could not query isobusfs-srv: " << process_error.message() << std::endl;
    }

    std::error_code wait_error;
    child.wait(wait_error);
    if (wait_error)
        std::cerr << "Could not reap isobusfs-srv: " << wait_error.message() << std::endl;
    state_->child.reset();
}

bool IsobusFsProcess::is_running() const noexcept
{
    if (!state_ || !state_->child)
        return false;
    std::error_code error;
    return state_->child->running(error) && !error;
}
}  // namespace rauch
