#pragma once

#include "boost_process.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rauch
{
std::unique_ptr<process::child> launch_isobusfs_process(const std::filesystem::path&    executable,
                                                        const std::vector<std::string>& arguments);
}  // namespace rauch
