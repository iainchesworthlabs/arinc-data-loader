// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/scenario_file.hpp"

#include "adl/arinc615a/limits.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>
#include <utility>

namespace adl::mock
{

namespace
{

using adl::scenario::ReadError;
using adl::scenario::Trigger;
namespace json = adl::scenario::json;

constexpr unsigned bit(Trigger trigger) noexcept
{
    return 1U << static_cast<unsigned>(trigger);
}
constexpr unsigned with_request = bit(Trigger::request);
constexpr unsigned with_later = bit(Trigger::status_file) | bit(Trigger::after_ms);
constexpr unsigned with_any = with_request | with_later;

struct ActionInfo
{
    std::string_view word;
    Effect effect;
    unsigned triggers; // the triggers it goes with
};

constexpr std::array<ActionInfo, 9> actions{{
    {"wait", Effect::wait, with_request},
    {"refuse", Effect::refuse, with_request},
    {"invalid_init_file", Effect::invalid_init_file, with_request},
    {"drop_status_file", Effect::drop_status_file, with_later},
    {"abort", Effect::abort, with_later},
    {"silent", Effect::silent, with_later},
    {"corrupt_served", Effect::corrupt_served, with_any},
    {"wrong_checksum", Effect::wrong_checksum, with_later},
    {"delay", Effect::delay, with_any},
}};

constexpr std::array<std::string_view, 4> operation_words{"information", "upload", "download_media_defined",
                                                          "download_operator_defined"};

constexpr std::int64_t max_delay_ms = 60000;

[[nodiscard]] std::unexpected<ReadError> fail(std::string message)
{
    return std::unexpected(ReadError{std::move(message)});
}

[[nodiscard]] std::string in_quotes(std::string_view text)
{
    return "\"" + std::string{text} + "\"";
}

[[nodiscard]] const ActionInfo* find_action(std::string_view word) noexcept
{
    const auto found = std::ranges::find(actions, word, &ActionInfo::word);
    return found == actions.end() ? nullptr : &*found;
}

// "request", "status_file" or "after_ms": the triggers in `mask`, in a sentence.
[[nodiscard]] std::string trigger_list(unsigned mask)
{
    std::vector<std::string> names;
    for (const Trigger trigger : {Trigger::request, Trigger::status_file, Trigger::after_ms})
    {
        if ((mask & bit(trigger)) != 0)
        {
            names.push_back(in_quotes(adl::scenario::trigger_name(trigger)));
        }
    }
    std::string list;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        list += i == 0 ? "" : (i + 1 == names.size() ? " or " : ", ");
        list += names[i];
    }
    return list;
}

[[nodiscard]] std::expected<ScenarioAction, ReadError> read_action(const adl::scenario::Step& step,
                                                                   const ActionInfo& info, bool has_seed)
{
    static constexpr std::array<std::string_view, 1> only_action{"action"};
    static constexpr std::array<std::string_view, 2> wait_keys{"action", "seconds"};
    static constexpr std::array<std::string_view, 2> refuse_keys{"action", "code"};
    static constexpr std::array<std::string_view, 4> delay_keys{"action", "ms", "min_ms", "max_ms"};

    const json::Object& members = step.action_members;
    const std::size_t number = step.number;
    std::span<const std::string_view> keys = only_action;
    switch (info.effect)
    {
    case Effect::wait:
        keys = wait_keys;
        break;
    case Effect::refuse:
        keys = refuse_keys;
        break;
    case Effect::delay:
        keys = delay_keys;
        break;
    default:
        break;
    }
    if (const auto error = adl::scenario::only_keys(members, keys, number, "do"))
    {
        return std::unexpected(*error);
    }

    ScenarioAction action;
    action.effect = info.effect;
    switch (info.effect)
    {
    case Effect::wait:
    {
        const auto seconds =
            adl::scenario::whole_number(members, "seconds", 1, adl::arinc615a::max_wait_seconds, number);
        if (!seconds)
        {
            return std::unexpected(seconds.error());
        }
        action.seconds = static_cast<std::uint32_t>(*seconds);
        break;
    }
    case Effect::refuse:
    {
        const auto code = adl::scenario::optional_text(members, "code", 6, number);
        if (!code)
        {
            return std::unexpected(code.error());
        }
        if (code->has_value() && **code != "0x1000" && **code != "0x1002")
        {
            return std::unexpected(adl::scenario::not_taken(number, "code", "\"0x1000\" or \"0x1002\""));
        }
        action.code = code->has_value() && **code == "0x1002" ? 0x1002U : 0x1000U;
        break;
    }
    case Effect::delay:
    {
        const bool has_ms = json::find(members, "ms") != nullptr;
        const bool has_min = json::find(members, "min_ms") != nullptr;
        const bool has_max = json::find(members, "max_ms") != nullptr;
        if (has_ms && (has_min || has_max))
        {
            return fail("step " + std::to_string(number) +
                        ": \"delay\" takes \"ms\", or \"min_ms\" and \"max_ms\", not both");
        }
        if (!has_ms && !(has_min && has_max))
        {
            return fail("step " + std::to_string(number) + ": \"delay\" needs \"ms\", or \"min_ms\" and \"max_ms\"");
        }
        if (has_ms)
        {
            const auto ms = adl::scenario::whole_number(members, "ms", 1, max_delay_ms, number);
            if (!ms)
            {
                return std::unexpected(ms.error());
            }
            action.min_ms = action.max_ms = static_cast<std::uint32_t>(*ms);
        }
        else
        {
            const auto least = adl::scenario::whole_number(members, "min_ms", 0, max_delay_ms, number);
            if (!least)
            {
                return std::unexpected(least.error());
            }
            const auto most = adl::scenario::whole_number(members, "max_ms", *least, max_delay_ms, number);
            if (!most)
            {
                return std::unexpected(most.error());
            }
            if (!has_seed)
            {
                return std::unexpected(adl::scenario::missing_seed(number, "min_ms"));
            }
            action.min_ms = static_cast<std::uint32_t>(*least);
            action.max_ms = static_cast<std::uint32_t>(*most);
            action.ranged = true;
        }
        break;
    }
    default:
        break;
    }
    return action;
}

[[nodiscard]] std::expected<ScenarioStep, ReadError> read_step(const adl::scenario::Step& step, bool has_seed)
{
    ScenarioStep out;
    out.number = step.number;
    out.trigger = step.when.trigger;
    out.at = step.when.at;
    out.every = step.when.every;
    out.times = step.when.times;
    out.per_mille = step.per_mille;
    out.comment = step.comment;

    if (const json::Value* operation = json::find(step.when.rest, "operation"))
    {
        const std::string* word = operation->as_string();
        const auto found = word == nullptr ? operation_words.end() : std::ranges::find(operation_words, *word);
        if (found == operation_words.end())
        {
            return std::unexpected(adl::scenario::not_taken(
                step.number, "operation",
                "one of \"information\", \"upload\", \"download_media_defined\" or \"download_operator_defined\""));
        }
        out.operation = static_cast<OperationKind>(found - operation_words.begin());
    }

    const ActionInfo* info = find_action(step.action);
    if (info == nullptr)
    {
        return fail("step " + std::to_string(step.number) + ": the action " + in_quotes(step.action) +
                    " is not one this version knows (it may be for a newer version)");
    }
    if ((info->triggers & bit(step.when.trigger)) == 0)
    {
        return fail("step " + std::to_string(step.number) + ": " + in_quotes(info->word) + " does not go with a " +
                    in_quotes(adl::scenario::trigger_name(step.when.trigger)) + " trigger; it goes with " +
                    trigger_list(info->triggers));
    }
    auto action = read_action(step, *info, has_seed);
    if (!action)
    {
        return std::unexpected(action.error());
    }
    out.action = *action;
    return out;
}

void append_text(std::string& out, std::string_view key, std::string_view text, std::string_view indent)
{
    out += indent;
    out += in_quotes(key);
    out += ": ";
    out += json::quote(text);
    out += ",\n";
}

[[nodiscard]] std::string when_text(const ScenarioStep& step)
{
    std::string text = "{ " + in_quotes(adl::scenario::trigger_name(step.trigger)) + ": " + std::to_string(step.at);
    if (step.every)
    {
        text += ", \"every\": " + std::to_string(*step.every);
    }
    if (step.times)
    {
        text += ", \"times\": " + std::to_string(*step.times);
    }
    if (step.operation)
    {
        text += ", \"operation\": " + in_quotes(operation_word(*step.operation));
    }
    return text + " }";
}

[[nodiscard]] std::string do_text(const ScenarioAction& action)
{
    std::string text = "{ \"action\": " + in_quotes(effect_word(action.effect));
    switch (action.effect)
    {
    case Effect::wait:
        text += ", \"seconds\": " + std::to_string(action.seconds);
        break;
    case Effect::refuse:
        text += action.code == 0x1002U ? ", \"code\": \"0x1002\"" : ", \"code\": \"0x1000\"";
        break;
    case Effect::delay:
        if (action.ranged)
        {
            text += ", \"min_ms\": " + std::to_string(action.min_ms) + ", \"max_ms\": " + std::to_string(action.max_ms);
        }
        else
        {
            text += ", \"ms\": " + std::to_string(action.min_ms);
        }
        break;
    default:
        break;
    }
    return text + " }";
}

} // namespace

std::string_view effect_word(Effect effect) noexcept
{
    const auto found = std::ranges::find(actions, effect, &ActionInfo::effect);
    return found == actions.end() ? std::string_view{} : found->word;
}

std::string_view operation_word(OperationKind kind) noexcept
{
    return operation_words[static_cast<std::size_t>(kind)];
}

std::expected<Scenario, ScenarioError> read_scenario(std::string_view text)
{
    static constexpr std::array<std::string_view, 1> when_keys{"operation"};
    const adl::scenario::Family family{adl::scenario::mock_kind, when_keys, {}};
    const auto envelope = adl::scenario::read_envelope(text, family);
    if (!envelope)
    {
        return std::unexpected(envelope.error());
    }
    Scenario scenario;
    scenario.name = envelope->name;
    scenario.description = envelope->description;
    scenario.seed = envelope->seed;
    scenario.steps.reserve(envelope->steps.size());
    for (const adl::scenario::Step& step : envelope->steps)
    {
        auto read = read_step(step, envelope->seed.has_value());
        if (!read)
        {
            return std::unexpected(read.error());
        }
        scenario.steps.push_back(std::move(*read));
    }
    return scenario;
}

std::string write_scenario(const Scenario& scenario)
{
    std::string out = "{\n  \"kind\": " + in_quotes(adl::scenario::mock_kind) + ",\n  \"format\": 1,\n";
    if (!scenario.name.empty())
    {
        append_text(out, "name", scenario.name, "  ");
    }
    if (!scenario.description.empty())
    {
        append_text(out, "description", scenario.description, "  ");
    }
    if (scenario.seed)
    {
        out += "  \"seed\": " + std::to_string(*scenario.seed) + ",\n";
    }
    if (scenario.steps.empty())
    {
        return out + "  \"steps\": []\n}\n";
    }
    out += "  \"steps\": [\n";
    for (std::size_t i = 0; i < scenario.steps.size(); ++i)
    {
        const ScenarioStep& step = scenario.steps[i];
        out += "    {\n";
        if (!step.comment.empty())
        {
            append_text(out, "comment", step.comment, "      ");
        }
        out += "      \"when\": " + when_text(step) + ",\n";
        if (step.per_mille)
        {
            out += "      \"per_mille\": " + std::to_string(*step.per_mille) + ",\n";
        }
        out += "      \"do\": " + do_text(step.action) + "\n";
        out += i + 1 == scenario.steps.size() ? "    }\n" : "    },\n";
    }
    return out + "  ]\n}\n";
}

std::expected<Scenario, ScenarioError> load_scenario_file(const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error)
    {
        return fail("the file cannot be read (" + error.message() + ")");
    }
    if (size > adl::scenario::max_file_size)
    {
        return fail("the file is larger than " + std::to_string(adl::scenario::max_file_size) + " bytes");
    }
    std::ifstream in(path, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!in || !in.read(text.data(), static_cast<std::streamsize>(text.size())) ||
        static_cast<std::uintmax_t>(in.gcount()) != size)
    {
        return fail("the file cannot be read");
    }
    return read_scenario(text);
}

} // namespace adl::mock
