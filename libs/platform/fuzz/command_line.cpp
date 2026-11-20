// SPDX-License-Identifier: GPL-3.0-or-later
#include "detail/command_line.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace
{

using namespace adl::platform::detail;

// What has to hold whatever the input is: text that comes out of the conversions is UTF-8, so going to UTF-16
// and back leaves it as it is. A failure is a crash, which is how a harness reports.
void require_round_trip(const std::string& utf8)
{
    if (utf16_to_utf8(utf8_to_utf16(utf8)) != utf8)
    {
        std::abort();
    }
}

} // namespace

// A command line, taken from the bytes as UTF-16 (two bytes a unit, low byte first, an odd last byte dropped),
// split and converted; and the same bytes as UTF-8 text, converted to UTF-16 and back.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    std::u16string line;
    for (std::size_t i = 0; i + 1 < size; i += 2)
    {
        line += static_cast<char16_t>(data[i] | (data[i + 1] << 8));
    }
    for (const std::string& argument : arguments_from_command_line(line))
    {
        require_round_trip(argument);
    }

    const std::string text = size == 0 ? std::string{} : std::string{reinterpret_cast<const char*>(data), size};
    require_round_trip(utf16_to_utf8(utf8_to_utf16(text)));
    return 0;
}
