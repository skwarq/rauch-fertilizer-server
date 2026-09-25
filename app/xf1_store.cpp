#include "xf1_store.hpp"

#include "atomic_file.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace rauch
{
Xf1FileStore::Xf1FileStore(std::filesystem::path directory) : directory_(std::move(directory))
{
}

std::filesystem::path Xf1FileStore::store(const Xf1Frame& frame, const Xf1Record& record)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::create_directories(directory_);
    const auto                        destination = directory_ / safe_filename(record);
    static std::atomic<std::uint64_t> sequence{ 0 };
    const auto                        temporary = destination.string() + ".tmp-"
                           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-"
                           + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    struct TemporaryFile final
    {
        std::filesystem::path path;
        ~TemporaryFile()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } cleanup{ temporary };
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("cannot create temporary XF1 file");
        output.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
        if (!output)
            throw std::runtime_error("cannot write XF1 file");
    }
    atomic_replace_file(temporary, destination);
    return destination;
}
}  // namespace rauch
