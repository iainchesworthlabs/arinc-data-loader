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
                                 [](const ScenarioStepFired&) { return std::string_view{"scenario_step"}; },
                                 [](const ScenarioDatagram&) { return std::string_view{"scenario_datagram"}; },
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
                              if (!started.features.empty())
                              {
                                  object.strings("scenario_features", started.features);
                              }
                              if (started.scenario)
                              {
                                  object.string("scenario", started.scenario->path)
                                      .number("scenario_steps", static_cast<std::int64_t>(started.scenario->steps));
                                  if (started.scenario->seed)
                                  {
                                      object.number("scenario_seed",
                                                    static_cast<std::int64_t>(*started.scenario->seed));
                                  }
                                  else
                                  {
                                      object.null("scenario_seed");
                                  }
                              }
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
                          [&object](const ScenarioStepFired& fired)
                          {
                              object.number("step", static_cast<std::int64_t>(fired.step))
                                  .string("trigger", fired.trigger)
                                  .number("at", fired.at)
                                  .string("operation", fired.operation)
                                  .string("action", fired.action)
                                  .boolean("applied", fired.applied);
                              if (fired.seconds)
                              {
                                  object.number("seconds", *fired.seconds);
                              }
                              if (fired.status)
                              {
                                  object.number("status", *fired.status);
                              }
                              if (fired.ms)
                              {
                                  object.number("ms", *fired.ms);
                              }
                              if (fired.copies)
                              {
                                  object.number("copies", *fired.copies);
                              }
                              if (fired.within_ms)
                              {
                                  object.number("within_ms", *fired.within_ms);
                              }
                              if (fired.count)
                              {
                                  object.number("count", *fired.count);
                              }
                              if (fired.datagram)
                              {
                                  object.string("datagram", *fired.datagram);
                              }
                              if (fired.file)
                              {
                                  object.string("file", *fired.file);
                              }
                              if (fired.block)
                              {
                                  object.number("block", *fired.block);
                              }
                          },
                          [&object](const ScenarioDatagram& hit)
                          {
                              object.number("step", static_cast<std::int64_t>(hit.step))
                                  .string("action", hit.action)
                                  .boolean("applied", hit.applied)
                                  .string("datagram", hit.datagram);
                              if (hit.file)
                              {
                                  object.string("file", *hit.file);
                              }
                              else
                              {
                                  object.null("file");
                              }
                              if (hit.block)
                              {
                                  object.number("block", *hit.block);
                              }
                              else
                              {
                                  object.null("block");
                              }
                              object.string("to", hit.to).number("size", static_cast<std::int64_t>(hit.size));
                          },
                          [&object](const Stopped& stopped)
                          {
                              object.string("reason", stopped.reason)
                                  .number("operations", static_cast<std::int64_t>(stopped.operations));
                              if (stopped.scenario_unused)
                              {
                                  const std::vector<std::int64_t> unused(stopped.scenario_unused->begin(),
                                                                         stopped.scenario_unused->end());
                                  object.numbers("scenario_unused", unused);
                              }
                              if (stopped.scenario_idle)
                              {
                                  const std::vector<std::int64_t> idle(stopped.scenario_idle->begin(),
                                                                       stopped.scenario_idle->end());
                                  object.numbers("scenario_idle", idle);
                              }
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
                              if (!started.features.empty())
                              {
                                  add(line, "scenario_features", joined(started.features, ','));
                              }
                              if (started.scenario)
                              {
                                  add(line, "scenario", started.scenario->path);
                                  add(line, "scenario_steps", std::to_string(started.scenario->steps));
                                  add(line, "scenario_seed",
                                      started.scenario->seed ? std::to_string(*started.scenario->seed) : "none");
                              }
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
                          [&line](const ScenarioStepFired& fired)
                          {
                              add(line, "step", std::to_string(fired.step));
                              add(line, "trigger", fired.trigger);
                              add(line, "at", std::to_string(fired.at));
                              add(line, "operation", fired.operation);
                              add(line, "action", fired.action);
                              add(line, "applied", fired.applied ? "true" : "false");
                              if (fired.seconds)
                              {
                                  add(line, "seconds", std::to_string(*fired.seconds));
                              }
                              if (fired.status)
                              {
                                  add(line, "status", std::to_string(*fired.status));
                              }
                              if (fired.ms)
                              {
                                  add(line, "ms", std::to_string(*fired.ms));
                              }
                              if (fired.copies)
                              {
                                  add(line, "copies", std::to_string(*fired.copies));
                              }
                              if (fired.within_ms)
                              {
                                  add(line, "within_ms", std::to_string(*fired.within_ms));
                              }
                              if (fired.count)
                              {
                                  add(line, "count", std::to_string(*fired.count));
                              }
                              if (fired.datagram)
                              {
                                  add(line, "datagram", *fired.datagram);
                              }
                              if (fired.file)
                              {
                                  add(line, "file", *fired.file);
                              }
                              if (fired.block)
                              {
                                  add(line, "block", std::to_string(*fired.block));
                              }
                          },
                          [&line](const ScenarioDatagram& hit)
                          {
                              add(line, "step", std::to_string(hit.step));
                              add(line, "action", hit.action);
                              add(line, "applied", hit.applied ? "true" : "false");
                              add(line, "datagram", hit.datagram);
                              add(line, "file", hit.file ? *hit.file : "none");
                              add(line, "block", hit.block ? std::to_string(*hit.block) : "none");
                              add(line, "to", hit.to);
                              add(line, "size", std::to_string(hit.size));
                          },
                          [&line](const Stopped& stopped)
                          {
                              add(line, "reason", stopped.reason);
                              add(line, "operations", std::to_string(stopped.operations));
                              if (stopped.scenario_unused)
                              {
                                  std::vector<std::string> unused;
                                  for (const std::size_t step : *stopped.scenario_unused)
                                  {
                                      unused.push_back(std::to_string(step));
                                  }
                                  add(line, "scenario_unused", unused.empty() ? "none" : joined(unused, ','));
                              }
                              if (stopped.scenario_idle)
                              {
                                  std::vector<std::string> idle;
                                  for (const std::size_t step : *stopped.scenario_idle)
                                  {
                                      idle.push_back(std::to_string(step));
                                  }
                                  add(line, "scenario_idle", idle.empty() ? "none" : joined(idle, ','));
                              }
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
