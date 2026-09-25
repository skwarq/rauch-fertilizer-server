#include "xf1.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace rauch
{
namespace
{
    std::uint16_t u16(const std::uint8_t* p)
    {
        return p[0] | (p[1] << 8);
    }
    std::uint32_t u32(const std::uint8_t* p)
    {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
               | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }
    std::string field(const Xf1Frame& f, std::size_t offset, std::size_t size)
    {
        std::size_t end = offset;
        while (end < offset + size && f[end] != 0)
            ++end;
        return { reinterpret_cast<const char*>(f.data() + offset), end - offset };
    }
}  // namespace

Xf1Record validate_and_decode(const Xf1Frame& frame)
{
    if (u32(frame.data()) != xf1_size)
        throw std::runtime_error("invalid XF1 length");
    if (u32(frame.data() + 8) != 3)
        throw std::runtime_error("unsupported XF1 version");
    if (u16(frame.data() + 12) != 1)
        throw std::runtime_error("unsupported XF1 record count");
    std::uint32_t sum = 0;
    for (std::size_t i = 16; i < frame.size(); ++i)
        sum += frame[i];
    if (u32(frame.data() + 4) != (sum & 0xffffu))
        throw std::runtime_error("XF1 checksum mismatch");
    return { field(frame, 53, 61), field(frame, 114, 61) };
}

std::filesystem::path safe_filename(const Xf1Record& record)
{
    std::string name = record.product.empty() ? "rauch-fertilizer" : record.product;
    while (!name.empty() && (name.back() == '.' || name.back() == ' '))
        name.pop_back();
    for (char& c : name)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x20 || byte == 0x7f || c == ' ' || c == '<' || c == '>' || c == ':' || c == '"' || c == '/'
            || c == '\\' || c == '|' || c == '?' || c == '*')
            c = '_';
    }
    if (name.empty() || name == "." || name == "..")
        name = "rauch-fertilizer";

    std::string basename = name.substr(0, name.find('.'));
    std::transform(basename.begin(), basename.end(), basename.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    static constexpr const char* reserved_names[] = { "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
                                                      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
                                                      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    for (const char* reserved : reserved_names)
    {
        if (basename == reserved)
        {
            name.insert(name.begin(), '_');
            break;
        }
    }
    return name + ".xf1";
}

}  // namespace rauch
