#include "process_launcher.hpp"

#if __has_include(<boost/process/v1.hpp>)
#    include "boost_process.hpp"

#    include <boost/process/v1/extend.hpp>
#else
#    include <boost/process/extend.hpp>
#endif
#include <windows.h>

namespace rauch
{
std::unique_ptr<process::child> launch_isobusfs_process(const std::filesystem::path&    executable,
                                                        const std::vector<std::string>& arguments)
{
    auto create_process_group =
        process::extend::on_setup([](auto& executor) { executor.creation_flags |= CREATE_NEW_PROCESS_GROUP; });
    return std::make_unique<process::child>(executable.string(), arguments, create_process_group);
}
}  // namespace rauch
