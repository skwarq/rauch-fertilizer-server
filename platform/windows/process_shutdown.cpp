#include "process_shutdown.hpp"

#include <windows.h>

namespace rauch
{
bool request_process_shutdown(std::uint64_t process_id) noexcept
{
    return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(process_id)) != 0;
}
}  // namespace rauch
