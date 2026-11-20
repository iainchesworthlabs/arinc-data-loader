// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/cli/events.hpp"

#include "adl/mock/cli/json.hpp"

#include <algorithm>
#include <cctype>

namespace adl::mock::cli
{

namespace
{

template <class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

[[nodiscard]] std::string joined(const std::vector<std::string>& values, char separator)
{
    std::string out;
    for (const std::string& value : values)
    {
        out += out.empty() ? "" : std::string(1, separator);
        out += value;
    }
    return out;
}

// A value in a plain line: as it is, or as a JSON string when it has a space, a quote, a control
// character or nothing in it, so that the line can still be split at the spaces.
[[nodiscard]] std::string plain_value(std::string_view value)
{
    const bool plain =
        !value.empty() && std::ranges::none_of(value,
                                               [](char c)
                                               {
                                                   const auto byte = static_cast<unsigned char>(c);
                                                   return byte <= 0x20U || byte >= 0x7FU || c == '"' || c == '\\';
                                               });
    return plain ? std::string{value} : json_string(value);
}

void add(std::string& line, std::string_view key, std::string_view value)
{
    line += ' ';
    line += key;
    line += '=';
    line += plain_value(value);
}

} // namespace

std::string_view event_name(const EventData& data) noexcept
{
    return std::visit(Overloaded{[](const Started&) { return std::string_view{"started"}; },
                                 [](const OperationBegan&) { return std::string_view{"operation_began"}; },
                                 [](const Progress&) { return std::string_view{"progress"}; },
                                 [](const UploadReceived&) { return std::string_view{"upload_received"}; },
                                 [](const OperationEnded&) { return std::string_view{"operation_ended"}; },
                                 [](const Stopped&) { return std::string_view{"stopped"}; }},
                      data);
}

std::string to_json_line(const Event& event)
{
    JsonObject object;
    object.string("event", event_name(event.data)).seconds("t", event.t);
    std::visit(Overloaded{[&object](const Started& started)
                          {
                              object.string("program", started.program)
                                  .string("version", started.version)
                                  .number("events", events_format)
                                  .string("thw_id_pos", started.thw_id_pos)
                                  .string("address", started.address)
                                  .number("find_port", started.find_port)
                                  .number("tftp_port", started.tftp_port)
                                  .strings("faults", started.faults);
                          },
                          [&object](const OperationBegan& began) { object.string("operation", began.operation); },
                          [&object](const Progress& progress)
                          { object.string("operation", progress.operation).number("percent", progress.percent); },
                          [&object](const UploadReceived& received)
                          {
                              object.string("part_number", received.part_number).strings("files", received.files);
                              if (received.stored_in)
                              {
                                  object.string("stored_in", *received.stored_in);
                              }
                              else
                              {
                                  object.null("stored_in");
                              }
                          },
                          [&object](const OperationEnded& ended)
                          {
                              object.string("operation", ended.operation);
                              if (ended.status)
                              {
                                  object.number("status", *ended.status);
                              }
                              else
                              {
                                  object.null("status");
                              }
                              object.string("status_name", ended.status_name).boolean("refused", ended.refused);
                          },
                          [&object](const Stopped& stopped)
                          {
                              object.string("reason", stopped.reason)
                                  .number("operations", static_cast<std::int64_t>(stopped.operations));
                          }},
               event.data);
    return object.text();
}

std::string to_plain_line(const Event& event)
{
    std::string line = seconds_text(event.t);
    line += ' ';
    line += event_name(event.data);
    std::visit(Overloaded{[&line](const Started& started)
                          {
                              add(line, "program", started.program);
                              add(line, "version", started.version);
                              add(line, "events", std::to_string(events_format));
                              add(line, "thw_id_pos", started.thw_id_pos);
                              add(line, "address", started.address);
                              add(line, "find_port", std::to_string(started.find_port));
                              add(line, "tftp_port", std::to_string(started.tftp_port));
                              add(line, "faults", started.faults.empty() ? "none" : joined(started.faults, ','));
                          },
                          [&line](const OperationBegan& began) { add(line, "operation", began.operation); },
                          [&line](const Progress& progress)
                          {
                              add(line, "operation", progress.operation);
                              add(line, "percent", std::to_string(progress.percent));
                          },
                          [&line](const UploadReceived& received)
                          {
                              add(line, "part_number", received.part_number);
                              add(line, "files", joined(received.files, ','));
                              add(line, "stored_in", received.stored_in ? *received.stored_in : "none");
                          },
                          [&line](const OperationEnded& ended)
                          {
                              add(line, "operation", ended.operation);
                              add(line, "status", ended.status ? std::to_string(*ended.status) : "none");
                              add(line, "status_name", ended.status_name);
                              add(line, "refused", ended.refused ? "true" : "false");
                          },
                          [&line](const Stopped& stopped)
                          {
                              add(line, "reason", stopped.reason);
                              add(line, "operations", std::to_string(stopped.operations));
                          }},
               event.data);
    return line;
}

std::string operation_id(std::string_view display_name)
{
    std::string id;
    for (const char c : display_name)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (std::isalnum(byte) != 0)
        {
            id.push_back(static_cast<char>(std::tolower(byte)));
        }
        else if (!id.empty() && id.back() != '_')
        {
            id.push_back('_');
        }
    }
    if (!id.empty() && id.back() == '_')
    {
        id.pop_back();
    }
    return id;
}

std::string_view status_name(adl::arinc615a::StatusCode code) noexcept
{
    using adl::arinc615a::StatusCode;
    switch (code)
    {
    case StatusCode::accepted:
        return "accepted";
    case StatusCode::in_progress:
        return "in_progress";
    case StatusCode::completed:
        return "completed";
    case StatusCode::in_progress_with_text:
        return "in_progress_with_text";
    case StatusCode::operation_denied:
        return "operation_denied";
    case StatusCode::operation_not_supported:
        return "operation_not_supported";
    case StatusCode::aborted_by_target:
        return "aborted_by_target";
    case StatusCode::aborted_by_data_loader:
        return "aborted_by_data_loader";
    case StatusCode::aborted_by_operator:
        return "aborted_by_operator";
    case StatusCode::item_failed:
        return "item_failed";
    }
    return "unknown";
}

} // namespace adl::mock::cli
