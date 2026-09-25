#include "tcp_server.hpp"
#include "xf1.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace rauch
{
namespace
{
    using boost::asio::ip::tcp;

    void cancel_timer(boost::asio::steady_timer& timer) noexcept
    {
        try
        {
            timer.cancel();
        }
        catch (...)
        {
        }
    }

    struct ClientSession
    {
        ClientSession(boost::asio::io_context& context, std::size_t session_id)
            : socket(context), deadline(context), id(session_id)
        {
        }

        tcp::socket               socket;
        boost::asio::steady_timer deadline;
        Xf1Frame                  frame{};
        std::size_t               id;
    };
}  // namespace

class TcpServer::Runtime
{
public:
    Runtime(std::size_t worker_count, std::size_t client_limit)
        : acceptor(context), signals(context), workers(worker_count), max_clients(client_limit)
    {
    }

    boost::asio::io_context                                         context;
    tcp::acceptor                                                   acceptor;
    boost::asio::signal_set                                         signals;
    boost::asio::thread_pool                                        workers;
    const std::size_t                                               max_clients;
    std::size_t                                                     next_id = 1;
    std::unordered_map<std::size_t, std::shared_ptr<ClientSession>> sessions;
    std::function<void()>                                           accept_next;
    std::mutex                                                      log_mutex;
    std::mutex                                                      session_mutex;
    std::atomic<bool>                                               stopped{ false };

    void shutdown() noexcept
    {
        stopped.store(true);
        boost::system::error_code ignored;
        signals.cancel(ignored);
        acceptor.close(ignored);
        std::lock_guard<std::mutex> lock(session_mutex);
        for (auto& entry : sessions)
        {
            cancel_timer(entry.second->deadline);
            entry.second->socket.cancel(ignored);
            entry.second->socket.close(ignored);
        }
    }
};

TcpServer::TcpServer(std::uint16_t port, std::filesystem::path output_directory, std::size_t worker_count,
                     std::size_t max_pending_clients, std::string listen_address, std::unique_ptr<IXf1Store> store)
    : port_(port), output_directory_(std::move(output_directory)), listen_address_(std::move(listen_address)),
      store_(store ? std::move(store) : std::make_unique<Xf1FileStore>(output_directory_)),
      worker_count_(worker_count == 0 ? std::clamp(std::thread::hardware_concurrency(), 2u, 8u) : worker_count),
      max_pending_clients_(max_pending_clients)
{
    if (worker_count_ == 0 || max_pending_clients_ == 0)
        throw std::invalid_argument("worker count and pending client limit must be positive");
}

TcpServer::~TcpServer()
{
    stop();
}

std::uint16_t TcpServer::bound_port() const noexcept
{
    return bound_port_.load();
}

void TcpServer::stop() noexcept
{
    stopping_.store(true);
    try
    {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        if (runtime_)
            boost::asio::post(runtime_->context, [runtime = runtime_.get()] { runtime->shutdown(); });
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        if (runtime_)
            runtime_->context.stop();
    }
}

int TcpServer::run()
{
    {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        if (runtime_)
            throw std::logic_error("TcpServer::run() can only be called once");
        stopping_.store(false);
        runtime_ = std::make_unique<Runtime>(worker_count_, max_pending_clients_);
    }
    auto& runtime = *runtime_;
    runtime.acceptor.open(tcp::v4());
    runtime.acceptor.set_option(tcp::acceptor::reuse_address(true));
    const auto address = boost::asio::ip::make_address_v4(listen_address_);
    runtime.acceptor.bind(tcp::endpoint(address, port_));
    runtime.acceptor.listen();
    bound_port_.store(runtime.acceptor.local_endpoint().port());
    runtime.signals.add(SIGINT);
    runtime.signals.add(SIGTERM);
    runtime.signals.async_wait(
        [this](const boost::system::error_code& error, int)
        {
            if (!error)
                stop();
        });

    const auto close_runtime = [&runtime]() noexcept { runtime.shutdown(); };

    const auto cleanup_sessions = [&runtime]() noexcept
    {
        std::lock_guard<std::mutex> lock(runtime.session_mutex);
        boost::system::error_code   ignored;
        for (auto& entry : runtime.sessions)
        {
            cancel_timer(entry.second->deadline);
            entry.second->socket.close(ignored);
        }
        runtime.sessions.clear();
    };

    runtime.accept_next = [this, &runtime]
    {
        if (stopping_.load() || runtime.stopped.load())
            return;
        auto session = std::make_shared<ClientSession>(runtime.context, runtime.next_id++);
        runtime.acceptor.async_accept(
            session->socket,
            [this, &runtime, session](const boost::system::error_code& error)
            {
                if (error)
                {
                    if (!stopping_.load() && error != boost::asio::error::operation_aborted)
                        std::cerr << "TCP accept failed: " << error.message() << '\n';
                    if (!stopping_.load() && error != boost::asio::error::operation_aborted)
                        stop();
                    return;
                }
                if (stopping_.load() || runtime.stopped.load())
                {
                    boost::system::error_code ignored;
                    session->socket.close(ignored);
                    return;
                }
                std::lock_guard<std::mutex> sessions_lock(runtime.session_mutex);
                if (runtime.sessions.size() >= runtime.max_clients)
                {
                    boost::system::error_code ignored;
                    session->socket.close(ignored);
                    boost::asio::post(runtime.context, [&runtime] { runtime.accept_next(); });
                    return;
                }
                runtime.sessions.emplace(session->id, session);
                session->deadline.expires_after(std::chrono::seconds(5));
                session->deadline.async_wait(
                    [session](const boost::system::error_code& timer_error)
                    {
                        if (!timer_error)
                        {
                            boost::system::error_code ignored;
                            session->socket.cancel(ignored);
                            session->socket.close(ignored);
                        }
                    });
                boost::asio::async_read(
                    session->socket, boost::asio::buffer(session->frame),
                    [this, &runtime, session](const boost::system::error_code& read_error, std::size_t)
                    {
                        cancel_timer(session->deadline);
                        if (read_error)
                        {
                            std::lock_guard<std::mutex> sessions_lock(runtime.session_mutex);
                            std::lock_guard<std::mutex> lock(runtime.log_mutex);
                            std::cerr << "Client " << session->id << " disconnected: " << read_error.message() << '\n';
                            runtime.sessions.erase(session->id);
                            return;
                        }
                        try
                        {
                            boost::asio::post(runtime.workers,
                                              [this, &runtime, session]
                                              {
                                                  try
                                                  {
                                                      const auto record = validate_and_decode(session->frame);
                                                      const auto path   = store_->store(session->frame, record);
                                                      std::lock_guard<std::mutex> lock(runtime.log_mutex);
                                                      std::cout << "Stored " << path << '\n';
                                                  }
                                                  catch (const std::exception& exception)
                                                  {
                                                      std::lock_guard<std::mutex> lock(runtime.log_mutex);
                                                      std::cerr << "Client " << session->id
                                                                << " XF1 rejected: " << exception.what() << '\n';
                                                  }
                                                  catch (...)
                                                  {
                                                      std::lock_guard<std::mutex> lock(runtime.log_mutex);
                                                      std::cerr << "Client " << session->id
                                                                << " XF1 processing failed with an unknown exception\n";
                                                  }
                                                  if (!runtime.stopped.load())
                                                  {
                                                      try
                                                      {
                                                          boost::asio::post(runtime.context,
                                                                            [&runtime, session]
                                                                            {
                                                                                std::lock_guard<std::mutex> lock(
                                                                                    runtime.session_mutex);
                                                                                runtime.sessions.erase(session->id);
                                                                            });
                                                      }
                                                      catch (...)
                                                      {
                                                      }
                                                  }
                                              });
                        }
                        catch (const std::exception& exception)
                        {
                            std::lock_guard<std::mutex> lock(runtime.log_mutex);
                            std::cerr << "Could not schedule XF1 processing: " << exception.what() << '\n';
                            std::lock_guard<std::mutex> sessions_lock(runtime.session_mutex);
                            runtime.sessions.erase(session->id);
                        }
                    });
                boost::asio::post(runtime.context, [&runtime] { runtime.accept_next(); });
            });
    };
    runtime.accept_next();

    std::cout << "Listening on TCP/" << bound_port_.load() << ", output=" << output_directory_ << '\n';
    try
    {
        runtime.context.run();
    }
    catch (...)
    {
        close_runtime();
        runtime.workers.join();
        cleanup_sessions();
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        runtime_.reset();
        throw;
    }
    close_runtime();
    runtime.workers.join();
    cleanup_sessions();
    {
        std::lock_guard<std::mutex> lock(runtime_mutex_);
        runtime_.reset();
    }
    return 0;
}
}  // namespace rauch
