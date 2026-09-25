#pragma once

#include <cstdint>

namespace rauch
{
bool request_process_shutdown(std::uint64_t process_id) noexcept;
}  // namespace rauch
