// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_scenario/reader.hpp"

#include "adl/scenario/envelope.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace adl::loader_scenario
{
namespace
{

namespace json = adl::licence::json;

using Failure = std::unexpected<ReadError>;

[[nodiscard]] Failure fail(std::string message)
{
    return Failure{ReadError{std::move(message)}};
}

[[nodiscard]] Failure fail(const adl::scenario::ReadError& error)
{
    return Failure{ReadError{error.message}};
}

[[nodiscard]] std::string quoted(std::string_view text)
{
    return "\"" + std::string{text} + "\"";
}

// "step 3: ", to put before a sentence about a step.
[[nodiscard]] std::string where(std::size_t step)
{
    return "step " + std::to_string(step) + ": ";
}

// The loader's own keys beside the shared ones.
constexpr std::array<std::string_view, 1> when_keys{"operation"};
constexpr std::array<std::string_view, 2> step_keys{"on", "count"};

constexpr std::int64_t max_count = adl::scenario::max_occurrence;

// ---- when -----------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<When, ReadError> read_when(const adl::scenario::When& shared, std::size_t number)
{
    When when;
    switch (shared.trigger)
    {
    case adl::scenario::Trigger::request:
        when.kind = TriggerKind::request;
        break;
    case adl::scenario::Trigger::status_file:
        when.kind = TriggerKind::status_file;
        break;
    case adl::scenario::Trigger::after_ms:
        when.kind = TriggerKind::after_ms;
        break;
    }
    when.value = static_cast<std::uint64_t>(shared.at);
    when.every = shared.every ? static_cast<std::uint64_t>(*shared.every) : 0;
    when.times = shared.times ? static_cast<std::uint64_t>(*shared.times) : 0;
    if (const json::Value* operation = json::find(shared.rest, "operation"))
    {
        const std::string* text = operation->as_string();
        const auto parsed = text != nullptr ? operation_from_string(*text) : std::nullopt;
        if (!parsed || *parsed == OperationName::any)
        {
            return fail(adl::scenario::not_taken(number, "operation",
                                                 "\"information\", \"upload\", \"download_media_defined\" or "
                                                 "\"download_operator_defined\""));
        }
        when.operation = *parsed;
    }
    return when;
}

// ---- on -------------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<Selector, ReadError> read_on(const json::Value* value, std::size_t number)
{
    Selector selector;
    if (value == nullptr)
    {
        return selector;
    }
    const json::Object* object = value->as_object();
    if (object == nullptr)
    {
        return fail(adl::scenario::not_taken(number, "on", "an object"));
    }
    static constexpr std::array<std::string_view, 3> keys{"datagram", "file", "block"};
    if (const auto error = adl::scenario::only_keys(*object, keys, number, "on"))
    {
        return fail(*error);
    }
    if (const json::Value* datagram = json::find(*object, "datagram"))
    {
        const std::string* text = datagram->as_string();
        const auto kind = text != nullptr ? datagram_kind_from_string(*text) : std::nullopt;
        if (!kind)
        {
            return fail(adl::scenario::not_taken(
                number, "datagram", "\"any\", \"request\", \"option_ack\", \"ack\", \"data\", \"error\" or \"reply\""));
        }
        selector.datagram = *kind;
    }
    if (json::find(*object, "file") != nullptr)
    {
        const auto file = adl::scenario::text(*object, "file", max_file_selector_chars, number);
        if (!file)
        {
            return fail(file.error());
        }
        selector.file = *file;
    }
    const auto block = adl::scenario::optional_whole_number(*object, "block", 0, 65535, number);
    if (!block)
    {
        return fail(block.error());
    }
    if (*block)
    {
        selector.block = static_cast<std::uint16_t>(**block);
    }
    return selector;
}

// ---- do -------------------------------------------------------------------------------------------------------

// "0x1004": 0x and four hexadecimal digits.
[[nodiscard]] std::optional<unsigned> status_from_text(std::string_view text) noexcept
{
    if (text.size() != 6 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X'))
    {
        return std::nullopt;
    }
    unsigned value = 0;
    for (const char c : text.substr(2))
    {
        value <<= 4U;
        if (c >= '0' && c <= '9')
        {
            value |= static_cast<unsigned>(c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            value |= static_cast<unsigned>(c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            value |= static_cast<unsigned>(c - 'A' + 10);
        }
        else
        {
            return std::nullopt;
        }
    }
    return value;
}

[[nodiscard]] std::string status_text(unsigned status)
{
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string text = "0x";
    for (int shift = 12; shift >= 0; shift -= 4)
    {
        text.push_back(hex[(status >> static_cast<unsigned>(shift)) & 0xFU]);
    }
    return text;
}

// Reads the members of `do` that an action has, and refuses any other.
class Members
{
public:
    Members(const json::Object& object, std::size_t number, std::string_view action)
        : object_(object), number_(number), action_(action)
    {
    }

    // A whole number from `low` to `high`; `assign` is given it when it is there. The key is named, so that it is not
    // counted as one the action does not take.
    template <typename Assign>
    [[nodiscard]] std::optional<ReadError> number(std::string_view key, std::int64_t low, std::int64_t high,
                                                  bool required, Assign&& assign)
    {
        known_.push_back(key);
        if (required)
        {
            const auto value = adl::scenario::whole_number(object_, key, low, high, number_);
            if (!value)
            {
                return ReadError{value.error().message};
            }
            assign(*value);
            return std::nullopt;
        }
        const auto value = adl::scenario::optional_whole_number(object_, key, low, high, number_);
        if (!value)
        {
            return ReadError{value.error().message};
        }
        if (*value)
        {
            assign(**value);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<ReadError> status(std::string_view key, bool required, unsigned& into)
    {
        known_.push_back(key);
        const json::Value* value = json::find(object_, key);
        if (value == nullptr)
        {
            if (required)
            {
                return ReadError{where(number_) + quoted(key) + " is missing; it is text such as \"0x1004\""};
            }
            return std::nullopt;
        }
        const std::string* text = value->as_string();
        const auto status = text != nullptr ? status_from_text(*text) : std::nullopt;
        if (!status)
        {
            return ReadError{where(number_) + quoted(key) +
                             " must be text such as \"0x1004\": 0x and four hexadecimal digits"};
        }
        into = *status;
        return std::nullopt;
    }

    void know(std::string_view key)
    {
        known_.push_back(key);
    }

    [[nodiscard]] bool has(std::string_view key) const
    {
        return json::find(object_, key) != nullptr;
    }

    // A key the action does not take.
    [[nodiscard]] std::optional<ReadError> unknown() const
    {
        for (const json::Member& member : object_)
        {
            if (member.key != "action" && std::ranges::find(known_, member.key) == known_.end())
            {
                return ReadError{where(number_) + quoted(action_) + " does not take the key " + quoted(member.key)};
            }
        }
        return std::nullopt;
    }

private:
    const json::Object& object_;
    std::size_t number_;
    std::string_view action_;
    std::vector<std::string_view> known_;
};

[[nodiscard]] std::expected<Fault, ReadError> read_do(const adl::scenario::Step& shared, bool has_seed)
{
    const std::size_t number = shared.number;
    const std::optional<Action> action = action_from_string(shared.action);
    if (!action)
    {
        return fail(where(number) + "\"action\" is " + quoted(shared.action) + ", which this version does not know");
    }
    Fault fault;
    fault.action = *action;
    Members members(shared.action_members, number, shared.action);
    std::optional<ReadError> problem;

    switch (*action)
    {
    case Action::drop:
    case Action::check_value:
        break;
    case Action::duplicate:
        problem =
            members.number("copies", 2, 16, false, [&](std::int64_t n) { fault.copies = static_cast<unsigned>(n); });
        break;
    case Action::delay:
    {
        const bool fixed = members.has("ms");
        const bool range = members.has("min_ms") || members.has("max_ms");
        if (fixed && range)
        {
            return fail(where(number) + "\"delay\" takes \"ms\", or \"min_ms\" and \"max_ms\", not both");
        }
        if (!fixed && !range)
        {
            return fail(where(number) + "\"delay\" needs \"ms\", or \"min_ms\" and \"max_ms\"");
        }
        if (fixed)
        {
            problem = members.number("ms", 1, max_delay_milliseconds, true,
                                     [&](std::int64_t n)
                                     {
                                         fault.milliseconds = n;
                                         fault.min_milliseconds = n;
                                         fault.max_milliseconds = n;
                                     });
        }
        else
        {
            std::int64_t low = 0;
            std::int64_t high = 0;
            problem = members.number("min_ms", 0, max_delay_milliseconds, true, [&](std::int64_t n) { low = n; });
            if (!problem)
            {
                problem = members.number("max_ms", 0, max_delay_milliseconds, true, [&](std::int64_t n) { high = n; });
            }
            if (!problem && low > high)
            {
                return fail(where(number) + "\"min_ms\" must not be more than \"max_ms\"");
            }
            if (!problem && low < high && !has_seed)
            {
                return fail(adl::scenario::missing_seed(number, "min_ms"));
            }
            fault.milliseconds = low;
            fault.min_milliseconds = low;
            fault.max_milliseconds = high;
        }
        break;
    }
    case Action::reorder:
        problem = members.number("within_ms", 1, max_delay_milliseconds, false,
                                 [&](std::int64_t n) { fault.milliseconds = n; });
        break;
    case Action::corrupt:
        problem =
            members.number("offset", 0, 65535, false, [&](std::int64_t n) { fault.offset = static_cast<unsigned>(n); });
        if (!problem)
        {
            problem = members.number("xor", 1, 255, false,
                                     [&](std::int64_t n) { fault.xor_mask = static_cast<unsigned>(n); });
        }
        break;
    case Action::length:
        problem =
            members.number("delta", -65535, 65535, true, [&](std::int64_t n) { fault.delta = static_cast<int>(n); });
        if (!problem && fault.delta == 0)
        {
            return fail(where(number) + "\"delta\" must not be 0");
        }
        break;
    case Action::error:
    {
        problem =
            members.number("code", 0, 65535, true, [&](std::int64_t n) { fault.code = static_cast<unsigned>(n); });
        members.know("text");
        if (!problem)
        {
            const auto text = adl::scenario::optional_text(shared.action_members, "text", max_error_text_chars, number);
            if (!text)
            {
                return fail(text.error());
            }
            if (*text)
            {
                fault.text = **text;
            }
        }
        break;
    }
    case Action::wait:
        problem = members.number("seconds", 1, 65535, true,
                                 [&](std::int64_t n) { fault.seconds = static_cast<unsigned>(n); });
        break;
    case Action::abort_message:
        problem = members.status("status", true, fault.status);
        break;
    case Action::abort:
    {
        problem = members.status("status", false, fault.status);
        if (!problem && fault.status != 0x1004 && fault.status != 0x1005)
        {
            return fail(where(number) + "an abort of the loader's own has \"status\" \"0x1004\" or \"0x1005\"");
        }
        break;
    }
    case Action::silent:
        problem = members.number("for_ms", 1, adl::scenario::max_after_ms, false,
                                 [&](std::int64_t n) { fault.milliseconds = n; });
        break;
    }
    if (problem)
    {
        return std::unexpected(*problem);
    }
    if (const auto unknown = members.unknown())
    {
        return std::unexpected(*unknown);
    }
    return fault;
}

// ---- a step ---------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<Step, ReadError> read_step(const adl::scenario::Step& shared, bool has_seed)
{
    const std::size_t number = shared.number;
    Step step;
    step.comment = shared.comment;
    if (shared.per_mille)
    {
        step.per_mille = static_cast<unsigned>(*shared.per_mille);
    }
    auto when = read_when(shared.when, number);
    if (!when)
    {
        return std::unexpected(when.error());
    }
    step.when = *when;
    auto fault = read_do(shared, has_seed);
    if (!fault)
    {
        return std::unexpected(fault.error());
    }
    step.fault = *fault;

    const json::Value* on_value = json::find(shared.rest, "on");
    auto on = read_on(on_value, number);
    if (!on)
    {
        return std::unexpected(on.error());
    }
    step.on = *on;
    // A wait that does not say which datagram is the answer to a request, which is the one WAIT is for.
    const json::Object* on_object = on_value != nullptr ? on_value->as_object() : nullptr;
    const bool names_datagram = on_object != nullptr && json::find(*on_object, "datagram") != nullptr;
    if (step.fault.action == Action::wait && !names_datagram)
    {
        step.on.datagram = DatagramKind::reply;
    }

    const json::Value* count = json::find(shared.rest, "count");
    if (count != nullptr)
    {
        const std::string* word = count->as_string();
        const std::int64_t* n = count->as_integer();
        if (word != nullptr && *word == "all")
        {
            step.all = true;
        }
        else if (n != nullptr && *n >= 1 && *n <= max_count)
        {
            step.count = static_cast<unsigned>(*n);
        }
        else
        {
            return fail(where(number) + "\"count\" must be a whole number from 1 to " + std::to_string(max_count) +
                        " or \"all\"");
        }
    }

    // What the action goes with.
    const std::string action = quoted(to_string(step.fault.action));
    if (!acts_on_datagrams(step.fault.action))
    {
        if (on_value != nullptr || count != nullptr)
        {
            return fail(where(number) + action + " acts on the operation, so it takes no \"on\" or \"count\"");
        }
        if (step.when.every != 0 || step.when.times != 0)
        {
            return fail(where(number) + action + " happens once, so its \"when\" takes no \"every\" or \"times\"");
        }
    }
    if (step.fault.action == Action::check_value &&
        ((step.on.datagram != DatagramKind::any && step.on.datagram != DatagramKind::option_ack) || step.on.block))
    {
        return fail(where(number) + action +
                    " acts on an option acknowledgement, so \"on\" may give only a \"file\" (and \"datagram\" "
                    "\"option_ack\")");
    }
    return step;
}

} // namespace

std::expected<Scenario, ReadError> read_scenario(std::string_view text)
{
    const adl::scenario::Family family{adl::scenario::loader_kind, when_keys, step_keys};
    const auto envelope = adl::scenario::read_envelope(text, family);
    if (!envelope)
    {
        return fail(envelope.error());
    }
    Scenario scenario;
    scenario.name = envelope->name;
    scenario.description = envelope->description;
    scenario.seed = envelope->seed;
    for (const adl::scenario::Step& shared : envelope->steps)
    {
        auto step = read_step(shared, envelope->seed.has_value());
        if (!step)
        {
            return std::unexpected(step.error());
        }
        scenario.steps.push_back(std::move(*step));
    }
    return scenario;
}

// ---- Writing --------------------------------------------------------------------------------------------------

namespace
{

void add_member(std::string& line, std::string_view key, const std::string& value)
{
    if (!line.empty() && line.back() != '{')
    {
        line += ", ";
    }
    else if (!line.empty())
    {
        line += ' ';
    }
    line += json::quote(key) + ": " + value;
}

[[nodiscard]] std::string object_of(const std::string& members)
{
    return members.empty() ? std::string{"{}"} : members + " }";
}

[[nodiscard]] std::string write_when(const When& when)
{
    std::string members = "{";
    add_member(members, to_string(when.kind), std::to_string(when.value));
    if (when.every != 0)
    {
        add_member(members, "every", std::to_string(when.every));
    }
    if (when.times != 0)
    {
        add_member(members, "times", std::to_string(when.times));
    }
    if (when.operation != OperationName::any)
    {
        add_member(members, "operation", json::quote(to_string(when.operation)));
    }
    return object_of(members);
}

[[nodiscard]] std::string write_do(const Fault& fault)
{
    std::string members = "{";
    add_member(members, "action", json::quote(to_string(fault.action)));
    switch (fault.action)
    {
    case Action::drop:
    case Action::check_value:
        break;
    case Action::duplicate:
        add_member(members, "copies", std::to_string(fault.copies));
        break;
    case Action::delay:
        if (fault.min_milliseconds < fault.max_milliseconds)
        {
            add_member(members, "min_ms", std::to_string(fault.min_milliseconds));
            add_member(members, "max_ms", std::to_string(fault.max_milliseconds));
        }
        else
        {
            add_member(members, "ms", std::to_string(fault.milliseconds));
        }
        break;
    case Action::reorder:
        if (fault.milliseconds > 0)
        {
            add_member(members, "within_ms", std::to_string(fault.milliseconds));
        }
        break;
    case Action::corrupt:
        add_member(members, "offset", std::to_string(fault.offset));
        add_member(members, "xor", std::to_string(fault.xor_mask));
        break;
    case Action::length:
        add_member(members, "delta", std::to_string(fault.delta));
        break;
    case Action::error:
        add_member(members, "code", std::to_string(fault.code));
        if (!fault.text.empty())
        {
            add_member(members, "text", json::quote(fault.text));
        }
        break;
    case Action::wait:
        add_member(members, "seconds", std::to_string(fault.seconds));
        break;
    case Action::abort_message:
    case Action::abort:
        add_member(members, "status", json::quote(status_text(fault.status)));
        break;
    case Action::silent:
        if (fault.milliseconds > 0)
        {
            add_member(members, "for_ms", std::to_string(fault.milliseconds));
        }
        break;
    }
    return object_of(members);
}

[[nodiscard]] std::string write_on(const Step& step)
{
    std::string members = "{";
    // A wait with no datagram named is the answer to a request; one that is not has to say so.
    if (step.on.datagram != DatagramKind::any || step.fault.action == Action::wait)
    {
        add_member(members, "datagram", json::quote(to_string(step.on.datagram)));
    }
    if (!step.on.file.empty())
    {
        add_member(members, "file", json::quote(step.on.file));
    }
    if (step.on.block)
    {
        add_member(members, "block", std::to_string(*step.on.block));
    }
    return members == "{" ? std::string{} : object_of(members);
}

} // namespace

std::string write_scenario(const Scenario& scenario)
{
    std::string out = "{\n";
    out += "  \"kind\": " + json::quote(adl::scenario::loader_kind) + ",\n";
    out += "  \"format\": " + std::to_string(adl::scenario::format_number) + ",\n";
    if (!scenario.name.empty())
    {
        out += "  \"name\": " + json::quote(scenario.name) + ",\n";
    }
    if (!scenario.description.empty())
    {
        out += "  \"description\": " + json::quote(scenario.description) + ",\n";
    }
    if (scenario.seed)
    {
        out += "  \"seed\": " + std::to_string(*scenario.seed) + ",\n";
    }
    out += "  \"steps\": [";
    for (std::size_t i = 0; i < scenario.steps.size(); ++i)
    {
        const Step& step = scenario.steps[i];
        out += i == 0 ? "\n" : ",\n";
        out += "    { ";
        if (!step.comment.empty())
        {
            out += "\"comment\": " + json::quote(step.comment) + ", ";
        }
        out += "\"when\": " + write_when(step.when);
        if (const std::string on = write_on(step); acts_on_datagrams(step.fault.action) && !on.empty())
        {
            out += ", \"on\": " + on;
        }
        out += ", \"do\": " + write_do(step.fault);
        if (acts_on_datagrams(step.fault.action))
        {
            if (step.all)
            {
                out += ", \"count\": \"all\"";
            }
            else if (step.count != 1)
            {
                out += ", \"count\": " + std::to_string(step.count);
            }
        }
        if (step.per_mille)
        {
            out += ", \"per_mille\": " + std::to_string(*step.per_mille);
        }
        out += " }";
    }
    out += scenario.steps.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return out;
}

} // namespace adl::loader_scenario
