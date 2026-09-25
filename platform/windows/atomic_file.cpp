#include "atomic_file.hpp"

#include <system_error>
#include <windows.h>

namespace rauch
{
void atomic_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "Atomic file replacement failed");
}
}  // namespace rauch
