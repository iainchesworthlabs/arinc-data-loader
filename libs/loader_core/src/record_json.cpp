// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/record_json.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::loader_core
{

namespace
{

constexpr std::string_view replacement_character = "\xEF\xBF\xBD"; // U+FFFD

// How many bytes the UTF-8 sequence at the start of `text` takes, or 0 when what is there is not one
// (a lead byte that starts nothing, a sequence that ends early, an overlong form, a surrogate, a number
// above U+10FFFF). `text` is not empty.
[[nodiscard]] std::size_t utf8_sequence_length(std::string_view text) noexcept
{
    const auto at = [&text](std::size_t index) { return static_cast<unsigned char>(text[index]); };
    const auto continues = [&](std::size_t index, unsigned char low = 0x80, unsigned char high = 0xBF)
    { return index < text.size() && at(index) >= low && at(index) <= high; };

    const unsigned char lead = at(0);
    if (lead < 0x80)
    {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        return continues(1) ? 2 : 0;
    }
    if (lead >= 0xE0 && lead <= 0xEF)
    {
        // E0 and ED have a narrower second byte: no overlong form, no surrogate.
        const unsigned char low = lead == 0xE0 ? 0xA0 : 0x80;
        const unsigned char high = lead == 0xED ? 0x9F : 0xBF;
        return continues(1, low, high) && continues(2) ? 3 : 0;
    }
    if (lead >= 0xF0 && lead <= 0xF4)
    {
        const unsigned char low = lead == 0xF0 ? 0x90 : 0x80;
        const unsigned char high = lead == 0xF4 ? 0x8F : 0xBF;
        return continues(1, low, high) && continues(2) && continues(3) ? 4 : 0;
    }
    return 0;
}

// `text` as the inside of a JSON string: a quote, a backslash and the control characters escaped, the
// rest as it is, and U+FFFD for each byte that is not part of valid UTF-8.
void append_escaped(std::string& out, std::string_view text)
{
    static constexpr std::string_view hex = "0123456789abcdef";
    for (std::size_t i = 0; i < text.size();)
    {
        const char c = text[i];
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
            ++i;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
        {
            switch (c)
            {
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += "\\u00";
                out += hex[static_cast<unsigned char>(c) >> 4];
                out += hex[static_cast<unsigned char>(c) & 0x0F];
                break;
            }
            ++i;
        }
        else
        {
            const std::size_t length = utf8_sequence_length(text.substr(i));
            if (length == 0)
            {
                out += replacement_character;
                ++i;
            }
            else
            {
                out.append(text.substr(i, length));
                i += length;
            }
        }
    }
}

void append_padded(std::string& out, long long value, int width)
{
    std::string digits = std::to_string(value);
    out.append(digits.size() < static_cast<std::size_t>(width) ? static_cast<std::size_t>(width) - digits.size() : 0,
               '0');
    out += digits;
}

// "2026-11-19T10:15:30Z", or nothing when there is no time or its year is not in 0000 to 9999.
[[nodiscard]] std::optional<std::string> format_time(const RecordTime& time)
{
    using namespace std::chrono;
    constexpr sys_days first{year{0} / January / 1};
    constexpr sys_days after_last{year{9999} / December / 31};
    if (!time || *time < first || *time >= after_last + days{1})
    {
        return std::nullopt;
    }
    const sys_days day = floor<days>(*time);
    const year_month_day date{day};
    const hh_mm_ss<seconds> time_of_day{*time - day};
    std::string text;
    append_padded(text, static_cast<int>(date.year()), 4);
    text += '-';
    append_padded(text, static_cast<unsigned>(date.month()), 2);
    text += '-';
    append_padded(text, static_cast<unsigned>(date.day()), 2);
    text += 'T';
    append_padded(text, time_of_day.hours().count(), 2);
    text += ':';
    append_padded(text, time_of_day.minutes().count(), 2);
    text += ':';
    append_padded(text, time_of_day.seconds().count(), 2);
    text += 'Z';
    return text;
}

[[nodiscard]] std::string_view operation_word(adl::arinc615a::OperationKind kind) noexcept
{
    switch (kind)
    {
    case adl::arinc615a::OperationKind::information:
        return "information";
    case adl::arinc615a::OperationKind::upload:
        return "upload";
    case adl::arinc615a::OperationKind::download_media:
        return "download_media_defined";
    case adl::arinc615a::OperationKind::download_operator:
        return "download_operator_defined";
    }
    return "information";
}

// Writes JSON in the layout the file has: every member and element on a line of its own, indented two
// spaces for each level, with an empty object or list as {} or [].
class Writer
{
public:
    void begin_object(std::string_view key = {})
    {
        element(key);
        out_ += '{';
        levels_.push_back({});
    }
    void end_object()
    {
        close('}');
    }
    void begin_array(std::string_view key = {})
    {
        element(key);
        out_ += '[';
        levels_.push_back({});
    }
    void end_array()
    {
        close(']');
    }
    void string(std::string_view key, std::string_view value)
    {
        element(key);
        quote(value);
    }
    void number(std::string_view key, std::uint64_t value)
    {
        element(key);
        out_ += std::to_string(value);
    }
    void time(std::string_view key, const RecordTime& value)
    {
        element(key);
        if (const auto text = format_time(value))
        {
            quote(*text);
        }
        else
        {
            out_ += "null";
        }
    }
    void null(std::string_view key)
    {
        element(key);
        out_ += "null";
    }

    // The text, with the line feed that ends the file.
    [[nodiscard]] std::string finish()
    {
        out_ += '\n';
        return std::move(out_);
    }

private:
    struct Level
    {
        bool empty = true;
    };

    // What comes before a member or an element: the comma after the one before, a new line and the
    // indentation, and for a member its key.
    void element(std::string_view key)
    {
        if (!levels_.empty())
        {
            if (!levels_.back().empty)
            {
                out_ += ',';
            }
            levels_.back().empty = false;
            out_ += '\n';
            out_.append(2 * levels_.size(), ' ');
        }
        if (!key.empty())
        {
            quote(key);
            out_ += ": ";
        }
    }
    void close(char bracket)
    {
        const bool empty = levels_.back().empty;
        levels_.pop_back();
        if (!empty)
        {
            out_ += '\n';
            out_.append(2 * levels_.size(), ' ');
        }
        out_ += bracket;
    }
    void quote(std::string_view text)
    {
        out_ += '"';
        append_escaped(out_, text);
        out_ += '"';
    }

    std::string out_;
    std::vector<Level> levels_;
};

} // namespace

std::string record_to_json(const OperationRecord& record)
{
    Writer json;
    json.begin_object();
    json.number("format", record_format);
    json.string("kind", record_kind);

    json.begin_object("program");
    json.string("name", record.program_name);
    json.string("version", record.program_version);
    json.end_object();

    json.string("operation", operation_word(record.operation));

    json.begin_object("target");
    json.string("address", record.target_address);
    json.string("position", record.target_position);
    json.end_object();

    json.time("started_at", record.started_at);
    json.time("ended_at", record.ended_at);

    json.string("result", record.result);
    json.string("reason", record.reason);
    if (record.status_code)
    {
        json.number("status_code", static_cast<std::uint16_t>(*record.status_code));
    }
    else
    {
        json.null("status_code");
    }
    json.string("detail", record.detail);

    if (record.media_set)
    {
        json.begin_object("media_set");
        json.string("path", record.media_set->path);
        json.string("part_number", record.media_set->part_number);
        json.number("members", record.media_set->members);
        json.end_object();
    }
    else
    {
        json.null("media_set");
    }

    json.begin_array("loads");
    for (const RecordedLoad& load : record.loads)
    {
        json.begin_object();
        json.string("part_number", load.part_number);
        json.string("header_file", load.header_file);
        json.string("status", status_name(load.status));
        json.number("percent", load.percent);
        json.end_object();
    }
    json.end_array();

    if (record.configuration)
    {
        json.begin_array("configuration");
        for (const RecordedHardware& hardware : *record.configuration)
        {
            json.begin_object();
            json.string("literal_name", hardware.literal_name);
            json.string("serial_number", hardware.serial_number);
            json.begin_array("parts");
            for (const RecordedPart& part : hardware.parts)
            {
                json.begin_object();
                json.string("part_number", part.part_number);
                json.string("amendment", part.amendment);
                json.string("designation", part.designation);
                json.end_object();
            }
            json.end_array();
            json.end_object();
        }
        json.end_array();
    }
    else
    {
        json.null("configuration");
    }

    json.begin_array("files");
    for (const RecordedFile& file : record.files)
    {
        json.begin_object();
        json.string("name", file.name);
        json.number("size", file.size);
        json.string("path", file.path);
        json.end_object();
    }
    json.end_array();

    if (record.download_folder)
    {
        json.string("download_folder", *record.download_folder);
    }
    else
    {
        json.null("download_folder");
    }

    json.null("licence");

    json.begin_array("log");
    for (const RecordedLogLine& line : record.log)
    {
        json.begin_object();
        json.time("at", line.at);
        json.string("severity", to_string(line.severity));
        json.string("text", line.text);
        json.end_object();
    }
    json.end_array();

    json.end_object();
    return json.finish();
}

} // namespace adl::loader_core
