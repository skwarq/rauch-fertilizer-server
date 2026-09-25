#include "atomic_file.hpp"

#include <system_error>

namespace rauch
{
void atomic_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    if (error)
        throw std::filesystem::filesystem_error("Atomic file replacement failed", source, destination, error);
}
}  // namespace rauch
