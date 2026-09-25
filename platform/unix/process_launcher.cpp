#include "process_launcher.hpp"

namespace rauch
{
std::unique_ptr<process::child> launch_isobusfs_process(const std::filesystem::path&    executable,
                                                        const std::vector<std::string>& arguments)
{
    return std::make_unique<process::child>(executable.string(), arguments);
}
}  // namespace rauch
