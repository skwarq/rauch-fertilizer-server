#include "process_shutdown.hpp"

#include <csignal>
#include <sys/types.h>
#include <unistd.h>

namespace rauch
{
bool request_process_shutdown(std::uint64_t process_id) noexcept
{
    return ::kill(static_cast<pid_t>(process_id), SIGTERM) == 0;
}
}  // namespace rauch
