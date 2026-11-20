// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/events.hpp"

#include "adl/load/json_line.hpp"
#include "adl/load/program.hpp"

#include <chrono>
#include <ostream>

namespace adl::load
{

namespace
{

// `text` for a terminal: a control character (a line feed in a file name, an escape sequence) is shown as `?`,
// whoever sent it. The two bytes of a C1 control character in UTF-8 are one `?`.
std::string printable(std::string_view text)
{
    std::string shown;
    shown.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte < 0x20 || byte == 0x7F)
        {
            shown += '?';
        }
        else if (byte == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) >= 0x80 &&
                 static_cast<unsigned char>(text[i + 1]) <= 0x9F)
        {
            shown += '?';
            ++i;
        }
        else
        {
            shown += static_cast<char>(byte);
        }
    }
    return shown;
}

std::string count_of(std::uint64_t count, std::string_view one, std::string_view many)
{
    return std::to_string(count) + ' ' + std::string{count == 1 ? one : many};
}

} // namespace

EventWriter::EventWriter(std::ostream& out, bool json, adl::platform::Clock& clock, adl::platform::Instant start)
    : out_(out), json_(json), clock_(clock), start_(start)
{
}

std::uint64_t EventWriter::elapsed_milliseconds()
{
    const auto elapsed = clock_.now() - start_;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    return milliseconds < 0 ? 0 : static_cast<std::uint64_t>(milliseconds);
}

void EventWriter::write(std::string_view line)
{
    out_ << line;
    out_.flush();
}

bool EventWriter::failed() const
{
    return !out_;
}

// The start of an event's object: `event`, `t`, and the id of the operation when the writer has one.
JsonLine EventWriter::json_event(std::string_view name, std::uint64_t milliseconds) const
{
    JsonLine line;
    line.string("event", name);
    line.milliseconds_as_seconds("t", milliseconds);
    if (id_)
    {
        line.string("id", *id_);
    }
    return line;
}

void EventWriter::serve_started(std::string_view address, std::uint16_t control_port, std::size_t max_operations)
{
    JsonLine line = json_event("started", elapsed_milliseconds());
    line.string("program", program_name);
    line.string("version", program_version);
    line.number("events", events_format);
    line.string("command", "serve");
    line.string("address", address);
    line.number("control_port", control_port);
    line.number("max_operations", max_operations);
    write(line.finish());
}

void EventWriter::request_refused(const std::optional<std::string>& id, const std::optional<std::string>& request,
                                  std::uint64_t line_number, std::string_view detail)
{
    JsonLine line = json_event("request_refused", elapsed_milliseconds());
    if (id)
    {
        line.string("id", *id);
    }
    else
    {
        line.null("id");
    }
    if (request)
    {
        line.string("request", *request);
    }
    else
    {
        line.null("request");
    }
    line.number("line", line_number);
    line.string("detail", detail);
    write(line.finish());
}

void EventWriter::stopped(std::string_view reason, std::uint64_t operations)
{
    JsonLine line = json_event("stopped", elapsed_milliseconds());
    line.string("reason", reason);
    line.number("operations", operations);
    write(line.finish());
}

void EventWriter::started(std::string_view command, const std::optional<std::string>& target,
                          const std::optional<std::string>& position)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("started", at);
        line.string("program", program_name);
        line.string("version", program_version);
        line.number("events", events_format);
        line.string("command", command);
        if (target)
        {
            line.string("target", *target);
        }
        if (position)
        {
            line.string("position", *position);
        }
        write(line.finish());
        return;
    }
    std::string text = std::string{program_name} + ' ' + std::string{program_version} + ": " + std::string{command};
    if (target)
    {
        text += ", target " + *target;
    }
    if (position)
    {
        text += " (" + printable(*position) + ')';
    }
    write(text + '\n');
}

void EventWriter::target_found(const FoundTarget& target)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("target_found", at);
        line.string("address", target.address);
        line.string("hardware_id", target.hardware_id);
        line.string("type_name", target.type_name);
        line.string("position", target.position);
        line.string("literal_name", target.literal_name);
        line.string("manufacturer_code", target.manufacturer_code);
        line.string("thw_id_pos", target.thw_id_pos);
        write(line.finish());
        return;
    }
    write("Found " + target.address + ": " + printable(target.thw_id_pos) + " (type " + printable(target.type_name) +
          ", \"" + printable(target.literal_name) + "\", manufacturer " + printable(target.manufacturer_code) + ")\n");
}

void EventWriter::media_set_checked(const CheckedMediaSet& media_set)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("media_set_checked", at);
        line.string("path", media_set.path);
        line.string("part_number", media_set.part_number);
        line.number("members", media_set.members);
        line.number("loads", media_set.loads);
        line.begin_array("findings");
        for (const std::string& finding : media_set.findings)
        {
            line.string_element(finding);
        }
        line.end_array();
        write(line.finish());
        return;
    }
    std::string text = "Checked media set " + printable(media_set.part_number) + " in " + printable(media_set.path) +
                       ": " + count_of(media_set.members, "member", "members") + ", " +
                       count_of(media_set.loads, "load", "loads") + ", " +
                       count_of(media_set.findings.size(), "finding", "findings");
    for (std::size_t i = 0; i < media_set.findings.size(); ++i)
    {
        text += (i == 0 ? ": " : "; ") + printable(media_set.findings[i]);
    }
    write(text + '\n');
}

void EventWriter::log(adl::loader_core::Severity severity, std::string_view text)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("log", at);
        line.string("severity", to_string(severity));
        line.string("text", text);
        write(line.finish());
        return;
    }
    write(std::string{to_string(severity)} + ": " + printable(text) + '\n');
}

void EventWriter::configuration(std::span<const adl::loader_core::RecordedHardware> hardware)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("configuration", at);
        line.begin_array("hardware");
        for (const adl::loader_core::RecordedHardware& item : hardware)
        {
            line.begin_object();
            line.string("literal_name", item.literal_name);
            line.string("serial_number", item.serial_number);
            line.begin_array("parts");
            for (const adl::loader_core::RecordedPart& part : item.parts)
            {
                line.begin_object();
                line.string("part_number", part.part_number);
                line.string("amendment", part.amendment);
                line.string("designation", part.designation);
                line.end_object();
            }
            line.end_array();
            line.end_object();
        }
        line.end_array();
        write(line.finish());
        return;
    }
    std::string text = "Configuration: " + count_of(hardware.size(), "hardware item", "hardware items");
    for (const adl::loader_core::RecordedHardware& item : hardware)
    {
        text += "; " + printable(item.literal_name) + " (serial number " + printable(item.serial_number) + "): ";
        text += item.parts.empty() ? "no parts" : "";
        for (std::size_t i = 0; i < item.parts.size(); ++i)
        {
            const adl::loader_core::RecordedPart& part = item.parts[i];
            text += (i == 0 ? "" : ", ") + printable(part.part_number);
            if (!part.amendment.empty())
            {
                text += ' ' + printable(part.amendment);
            }
        }
    }
    write(text + '\n');
}

void EventWriter::progress(unsigned percent, std::span<const adl::loader_core::RecordedLoad> loads)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("progress", at);
        line.number("percent", percent);
        line.begin_array("loads");
        for (const adl::loader_core::RecordedLoad& load : loads)
        {
            line.begin_object();
            line.string("part_number", load.part_number);
            line.string("header_file", load.header_file);
            line.string("status", adl::loader_core::status_name(load.status));
            line.number("percent", load.percent);
            line.end_object();
        }
        line.end_array();
        write(line.finish());
        return;
    }
    std::string text = "Progress: " + std::to_string(percent) + '%';
    for (const adl::loader_core::RecordedLoad& load : loads)
    {
        text += "; " + printable(load.part_number) + ' ' + std::string{adl::loader_core::status_name(load.status)} +
                ' ' + std::to_string(load.percent) + '%';
    }
    write(text + '\n');
}

void EventWriter::file_list(std::span<const ListedFile> files)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("file_list", at);
        line.begin_array("files");
        for (const ListedFile& file : files)
        {
            line.begin_object();
            line.string("name", file.name);
            line.string("description", file.description);
            line.end_object();
        }
        line.end_array();
        write(line.finish());
        return;
    }
    std::string text = "The target lists " + count_of(files.size(), "file", "files");
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        text += (i == 0 ? ": " : ", ") + printable(files[i].name);
    }
    write(text + '\n');
}

void EventWriter::file_received(const adl::loader_core::RecordedFile& file)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("file_received", at);
        line.string("name", file.name);
        line.number("size", file.size);
        line.string("path", file.path);
        write(line.finish());
        return;
    }
    write("Received " + printable(file.name) + ", " + count_of(file.size, "byte", "bytes") + ", in " +
          printable(file.path) + '\n');
}

void EventWriter::finished(const FinishedEvent& event)
{
    const std::uint64_t at = elapsed_milliseconds();
    if (json_)
    {
        JsonLine line = json_event("finished", at);
        line.string("result", event.completed ? "completed" : "failed");
        line.string("reason", event.reason);
        if (event.status_code)
        {
            line.number("status_code", *event.status_code);
        }
        else
        {
            line.null("status_code");
        }
        line.string("detail", event.detail);
        line.number("exit_code", static_cast<std::uint64_t>(event.exit_code));
        if (event.count)
        {
            line.number("count", *event.count);
        }
        if (event.is_download)
        {
            if (event.download_folder)
            {
                line.string("download_folder", *event.download_folder);
            }
            else
            {
                line.null("download_folder");
            }
        }
        if (event.has_record)
        {
            if (event.record)
            {
                line.string("record", *event.record);
            }
            else
            {
                line.null("record");
            }
        }
        write(line.finish());
        return;
    }
    std::string text =
        std::string{"Finished: "} + (event.completed ? "completed" : "failed, " + std::string{event.reason});
    text += " (exit code " + std::to_string(event.exit_code) + ")";
    if (!event.detail.empty())
    {
        text += ": " + printable(event.detail);
    }
    if (event.count)
    {
        text += " [" + count_of(*event.count, "target", "targets") + "]";
    }
    if (event.download_folder)
    {
        text += " [files in " + printable(*event.download_folder) + ']';
    }
    if (event.record)
    {
        text += " [record in " + printable(*event.record) + ']';
    }
    write(text + '\n');
}

} // namespace adl::load
