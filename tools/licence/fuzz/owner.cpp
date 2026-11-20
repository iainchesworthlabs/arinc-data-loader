// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The fuzz harness for what the owner's tool reads that did not come from the library: the public key file
// (--trust), the words of a command line, the UTF-16 command line of Windows, a licence number and a key id.
// Each must refuse a bad input with an error and never crash. What the key file reader accepts must be the text
// the writer produces, and must read again as the same key.

#include "commands.hpp"
#include "messages.hpp"
#include "options.hpp"
#include "public_key.hpp"
#include "utf8.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    namespace own = adl::licence_owner;

    const std::string_view text(reinterpret_cast<const char*>(data), size);

    // A public key file: what is accepted is exactly what the writer makes, and reads back as the same key.
    if (const auto key = own::parse_public_key(text))
    {
        const std::string written = own::format_public_key(*key);
        std::string_view expected = text;
        if (expected.ends_with('\n'))
        {
            expected.remove_suffix(1);
        }
        if (written != std::string(expected) + "\n")
        {
            std::abort();
        }
        const auto again = own::parse_public_key(written);
        if (!again || again->key != key->key || again->key_id != key->key_id || again->purpose != key->purpose)
        {
            std::abort();
        }
    }

    // The words of a command line: one on each line of the input, read as the options of a command that takes
    // all the kinds there are.
    std::vector<std::string> words;
    for (std::size_t start = 0; start <= text.size();)
    {
        const std::size_t end = text.find('\n', start);
        const std::size_t stop = end == std::string_view::npos ? text.size() : end;
        words.emplace_back(text.substr(start, stop - start));
        start = stop + 1;
    }
    static constexpr std::array<own::OptionSpec, 4> specs{
        {{"--test", false, false}, {"--trust", true, true}, {"--now", true, false}, {"--out", true, false}}};
    static_cast<void>(own::parse_options("verify", words, specs, 1));

    // The same bytes as UTF-16 units: a lone surrogate is refused, anything else gives text that is UTF-8.
    std::u16string units;
    for (std::size_t i = 0; i + 1 < size; i += 2)
    {
        units.push_back(static_cast<char16_t>(data[i] | (data[i + 1] << 8)));
    }
    if (const auto converted = own::utf16_to_utf8(units))
    {
        // Every unit that is not half of a pair is one to four bytes, so the text is at least as long.
        if (converted->size() < units.size())
        {
            std::abort();
        }
        static_cast<void>(own::path_from_utf8(*converted));
    }

    static_cast<void>(own::is_licence_number(text));
    static_cast<void>(own::is_test_licence_number(text));
    static_cast<void>(own::is_key_id(text));
    // A sentence with placeholders in it: the values are filled in where they are named, and the rest is kept.
    static_cast<void>(own::format(text, {"a", "b"}));
    return 0;
}
