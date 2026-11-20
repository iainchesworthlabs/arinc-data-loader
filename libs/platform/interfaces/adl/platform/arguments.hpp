// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace adl::platform
{

// The arguments of the running program as UTF-8 text, the program name first, as a `main` takes them:
//
//     int main(int argc, char** argv)
//     {
//         const std::vector<std::string> arguments = adl::platform::arguments(argc, argv);
//
// A command-line program takes its arguments from this function and does not read `argv` itself. On
// Windows `argv` is the command line narrowed to the system's code page, in which a name with a
// character outside the page (Japanese in a Western system) has already lost it, so Windows reads the
// command line of the process as UTF-16, splits it as the C run-time does, and converts each argument;
// a half of a surrogate pair becomes U+FFFD and nothing throws. Everywhere else the system passes the
// bytes the user typed and they are returned as they are, with no check that they are UTF-8. `argc` and
// `argv` are the arguments of `main`, and are all a platform with nothing better has. A null `argv`
// gives an empty list. One definition per platform.
[[nodiscard]] std::vector<std::string> arguments(int argc, const char* const* argv);

// A path from UTF-8 text, such as an argument returned by `arguments`: the reverse of `to_utf8` and
// `to_platform_path` in paths.hpp. Making a std::filesystem::path from a std::string reads the bytes in
// the system's code page on Windows, which gives another name, without an error; this reads them as
// UTF-8. Text that is not UTF-8 does not throw: on Windows each bad byte becomes U+FFFD, elsewhere the
// bytes are the name's, as the system has them. On a platform whose standard library has no real
// std::filesystem::path (INTEGRITY, docs/platform-ghs.md) the path is that library's. One definition per
// platform.
[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view text);

} // namespace adl::platform
