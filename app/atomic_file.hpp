#pragma once

#include <filesystem>

namespace rauch
{
void atomic_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination);
}  // namespace rauch
