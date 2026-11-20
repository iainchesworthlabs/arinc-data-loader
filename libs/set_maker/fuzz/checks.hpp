// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the harnesses of this directory check, as functions that say what is wrong instead of aborting, so that
// the harnesses (description.cpp and inspect_set.cpp, which abort) and the unit tests that replay their seeds
// (tests/make_set/fuzz_replay_test.cpp, which report) run the same code.

#include "adl/licence/json.hpp"
#include "adl/make_set/command.hpp"
#include "adl/make_set/description.hpp"
#include "adl/make_set/inspection.hpp"
#include "adl/make_set/options.hpp"
#include "adl/set_maker/inspect.hpp"
#include "adl/set_maker/validate.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::set_maker::fuzz
{

// The bytes of a description file, as a person or a program writes one. What is not a description is refused
// with a sentence. What is one is inside every limit of format 1, is a plan that the generator's rules accept,
// and is written out as a canonical description that reads back as the same one. Empty when all is as it
// should be.
[[nodiscard]] inline std::string check_description(std::string_view text)
{
    using namespace adl::make_set;

    const auto description = read_description(text, "root");
    if (!description)
    {
        if (description.error().sentences.empty() ||
            std::ranges::any_of(description.error().sentences, [](const std::string& s) { return s.empty(); }))
        {
            return "a refusal with no sentence";
        }
        return {};
    }

    const SetPlan& plan = description->plan;
    std::size_t files = 0;
    bool within = !plan.loads.empty() && plan.loads.size() <= max_loads && plan.members >= 1 && plan.members <= 255 &&
                  description->sources.size() == plan.loads.size();
    for (std::size_t i = 0; within && i < plan.loads.size(); ++i)
    {
        within = !plan.loads[i].files.empty() && plan.loads[i].files.size() <= max_files_in_a_load &&
                 plan.loads[i].target_hardware_ids.size() <= max_target_hardware_ids &&
                 description->sources[i].size() == plan.loads[i].files.size();
        files += plan.loads[i].files.size();
    }
    if (!within || files > max_files_in_all)
    {
        return "a description outside the limits was accepted";
    }
    if (!validate(plan).empty())
    {
        return "a description that the generator's rules refuse was accepted";
    }

    const std::string written = write_description(*description);
    const auto again = read_description(written, "root");
    if (!again)
    {
        return "the canonical description does not read: " + again.error().sentences.front();
    }
    if (write_description(*again) != written)
    {
        return "the canonical description does not read back as the same one";
    }
    return {};
}

// The command line of adl_make_set: the input is the arguments, each ended by a line feed. What cannot be used is
// refused with a sentence. What can is a command with the subject it needs, and with the options that belong to it
// only. Empty when all is as it should be.
[[nodiscard]] inline std::string check_options(std::string_view text)
{
    using namespace adl::make_set;

    std::vector<std::string> arguments;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        arguments.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    const std::vector<std::string_view> views(arguments.begin(), arguments.end());

    const auto options = parse_options(views);
    if (!options)
    {
        return options.error().sentence.empty() ? "a refusal with no sentence" : std::string{};
    }
    if (options->help || options->version)
    {
        return {};
    }
    if (options->subject.empty())
    {
        return "a command line with no subject was accepted";
    }
    if (options->command == Command::make ? !options->output.has_value()
                                          : (options->output.has_value() || options->source_root.has_value()))
    {
        return "the options of a command are not the ones that belong to it";
    }
    return {};
}

namespace detail
{

// A reader of the archive of files that check_members() takes.
class Cursor
{
public:
    explicit Cursor(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool take(std::size_t count, std::string& out)
    {
        if (bytes_.size() - position_ < count)
        {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(bytes_.data() + position_), count);
        position_ += count;
        return true;
    }
    [[nodiscard]] bool number(std::size_t width, std::uint32_t& out)
    {
        std::string text;
        if (!take(width, text))
        {
            return false;
        }
        out = 0;
        for (const char c : text)
        {
            out = (out << 8U) | static_cast<unsigned char>(c);
        }
        return true;
    }
    [[nodiscard]] std::size_t left() const noexcept
    {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

inline constexpr std::size_t most_files = 64;

} // namespace detail

// The members of a media set, which is what `adl_make_set inspect` reads once the folder is read. The input is a
// list of files, each:
//
//     u16 member number (big endian), u8 length and the bytes of the pathname, u8 length and the bytes of the
//     name, u32 size (big endian) and that many bytes (as many as there are, if the input ends first)
//
// until the input ends or a record is cut. Files with the same member number are the files of one member.
// What does not read as a media set is refused with a reason; what does is inspected, and the document that
// `inspect --json` writes for it has to be JSON that the strict reader reads. Empty when all is as it should be.
[[nodiscard]] inline std::vector<adl::arinc665::MediaMember> members_of(std::span<const std::uint8_t> bytes)
{
    detail::Cursor cursor{bytes};
    std::vector<adl::arinc665::MediaMember> members;
    for (std::size_t count = 0; count < detail::most_files; ++count)
    {
        std::uint32_t member_number = 0;
        std::uint32_t path_size = 0;
        std::uint32_t name_size = 0;
        std::uint32_t file_size = 0;
        adl::arinc665::MediaFile file;
        if (!cursor.number(2, member_number) || !cursor.number(1, path_size) ||
            !cursor.take(path_size, file.pathname) || !cursor.number(1, name_size) ||
            !cursor.take(name_size, file.name) || !cursor.number(4, file_size))
        {
            break;
        }
        std::string content;
        if (!cursor.take(std::min<std::size_t>(file_size, cursor.left()), content))
        {
            break;
        }
        for (const char c : content)
        {
            file.bytes.push_back(static_cast<std::byte>(c));
        }
        const auto member = std::ranges::find(members, static_cast<std::uint16_t>(member_number),
                                              &adl::arinc665::MediaMember::sequence_number);
        if (member == members.end())
        {
            members.push_back(adl::arinc665::MediaMember{static_cast<std::uint16_t>(member_number), {std::move(file)}});
        }
        else
        {
            member->files.push_back(std::move(file));
        }
    }
    return members;
}

[[nodiscard]] inline std::string check_members(std::span<const std::uint8_t> bytes)
{
    const std::vector<adl::arinc665::MediaMember> members = members_of(bytes);
    const auto inspected = inspect_members(members);
    if (!inspected)
    {
        return inspected.error().detail.empty() ? "a refusal with no reason" : std::string{};
    }

    // The document says what the inspection found, and is JSON that no reader can read two ways. The reader
    // allows eight levels and the document is at most five deep.
    const adl::make_set::Verdict verdict{true, "completed", "read", adl::make_set::exit_ok};
    const std::string document = adl::make_set::inspection_document(verdict, "folder", &*inspected);
    if (!adl::licence::json::parse_strict(document))
    {
        return "the inspection document is not JSON that the strict reader reads";
    }
    if (!std::ranges::is_sorted(inspected->members_present) || inspected->members_present.size() != members.size())
    {
        return "the members found are not the members given";
    }
    std::uint64_t held = 0;
    for (const adl::arinc665::MediaMember& member : members)
    {
        for (const adl::arinc665::MediaFile& file : member.files)
        {
            held += file.bytes.size();
        }
    }
    std::uint64_t counted = 0;
    for (const MemberSize& size : inspected->member_sizes)
    {
        counted += size.size;
    }
    if (inspected->member_sizes.size() != members.size() || counted != held)
    {
        return "the sizes of the members are not the bytes they hold";
    }
    return {};
}

} // namespace adl::set_maker::fuzz
