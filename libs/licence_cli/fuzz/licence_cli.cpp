// SPDX-License-Identifier: GPL-3.0-or-later
// The fuzz harness for the commands of adl_licence. The input is a command line, one word to a tab, up to the
// first line end, and then the bytes of a file. The file is put where every command can meet it: as the file
// the command line names, in the user's folder and in the folder of the computer as a licence and as a list of
// withdrawn licences, as the clock file, and signed with the test key as a licence and as a list, so that it
// gets past the signature check to the readers behind it. Whatever it is, a command must come back with an
// exit code of 0, 1 or 2, `show` and `verify` must write nothing, and nothing printed may hold a control
// character from a file.

#include "adl/licence_cli/licence_cli.hpp"

#include "adl/licence/licence.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"
#include "memory_files.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

[[nodiscard]] std::string signed_as(std::string_view purpose, std::string_view payload)
{
    namespace lic = adl::licence;
    const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return lic::signed_file::write(
        lic::signing::sign(purpose, lic::test_keys::key_id, bytes, lic::test_keys::private_key()));
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    namespace lic = adl::licence;
    namespace cli = adl::licence_cli;

    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const std::size_t line_end = text.find('\n');
    const std::string_view words = text.substr(0, line_end == std::string_view::npos ? text.size() : line_end);
    const std::string_view content =
        line_end == std::string_view::npos ? std::string_view{} : text.substr(line_end + 1);

    // At most eight words, so that a long line is not a long command line.
    std::vector<std::string> arguments;
    for (std::size_t start = 0; start <= words.size() && arguments.size() < 8;)
    {
        const std::size_t tab = words.find('\t', start);
        const std::size_t stop = tab == std::string_view::npos ? words.size() : tab;
        arguments.emplace_back(words.substr(start, stop - start));
        start = stop + 1;
    }
    if (words.empty())
    {
        arguments.clear();
    }

    adl::test::MemoryFiles files;
    files.put("f", content);
    files.put("home/licences/in-folder.adllic", content);
    files.put("home/licences/list.adlrev", content);
    files.put("machine/licences/in-machine.adllic", content);
    files.put("home/licences/signed.adllic", signed_as(lic::licence_purpose, content));
    files.put("home/licences/signed.adlrev", signed_as(lic::revocations_purpose, content));
    files.put("g", signed_as(lic::licence_purpose, content));
    files.put("home/licence-clock.json", content);

    cli::Environment environment;
    environment.files = &files;
    environment.folders = {"home/licences", "machine/licences", "home/licence-clock.json"};
    environment.computer_name = "LAB-RIG-07";
    const auto test_key = lic::test_keys::trusted();
    environment.keys.assign(test_key.begin(), test_key.end());
    environment.version = "fuzz";
    const lic::TimePoint now{std::chrono::sys_days{std::chrono::year{2027} / std::chrono::January / 1}};
    environment.now = [now]() -> std::optional<lic::TimePoint> { return now; };

    std::ostringstream out;
    std::ostringstream err;
    const int code = cli::run(arguments, environment, {out, err});
    if (code != cli::exit_ok && code != cli::exit_failed && code != cli::exit_usage)
    {
        std::abort();
    }
    const bool wrote = files.creates != 0 || files.replaces != 0 || files.removes != 0;
    const bool reads_only = arguments.empty() || arguments[0] == "show" || arguments[0] == "verify" ||
                            arguments[0] == "--help" || arguments[0] == "--version";
    if ((reads_only || code == cli::exit_usage) && wrote)
    {
        std::abort();
    }
    for (const std::string& printed : {out.str(), err.str()})
    {
        for (const char letter : printed)
        {
            const auto byte = static_cast<unsigned char>(letter);
            if ((byte < 0x20 && byte != '\n') || byte == 0x7F)
            {
                std::abort();
            }
        }
    }
    return 0;
}
