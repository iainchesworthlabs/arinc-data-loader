// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_scenario/reader.hpp"

#include "adl/licence/json.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace adl::loader_scenario
{
namespace
{

namespace json = adl::licence::json;

constexpr std::int64_t max_count = 1'000'000;
constexpr std::int64_t max_every = 1'000'000;
constexpr std::int64_t min_every_ms = 10;

using Failure = std::unexpected<ReadError>;

[[nodiscard]] Failure fail(std::string message)
{
    return Failure{ReadError{std::move(message)}};
}

[[nodiscard]] std::string quoted(std::string_view text)
{
    return "\"" + std::string{text} + "\"";
}

[[nodiscard]] std::string range_text(std::int64_t low, std::int64_t high)
{
    return "a whole number from " + std::to_string(low) + " to " + std::to_string(high);
}

// The members of an object, each taken once, so that what is left over is a key this version does not know.
class Fields
{
public:
    explicit Fields(const json::Object& object) : object_(object), taken_(object.size(), false) {}

    [[nodiscard]] const json::Value* take(std::string_view key)
    {
        for (std::size_t i = 0; i < object_.size(); ++i)
        {
            if (object_[i].key == key)
            {
                taken_[i] = true;
                return &object_[i].value;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<std::string> unknown() const
    {
        for (std::size_t i = 0; i < object_.size(); ++i)
        {
            if (!taken_[i])
            {
                return object_[i].key;
            }
        }
        return std::nullopt;
    }

private:
    const json::Object& object_;
    std::vector<bool> taken_;
};

// "step 3: " or nothing, to put before a sentence.
struct Where
{
    std::size_t step = 0; // from 1; 0 for the top level

    [[nodiscard]] std::string prefix() const
    {
        return step == 0 ? std::string{} : "step " + std::to_string(step) + ": ";
    }
};

using Integer = std::expected<std::optional<std::int64_t>, ReadError>;

// A whole number in [low, high], or none when the key is not there.
[[nodiscard]] Integer optional_integer(Fields& fields, std::string_view key, std::int64_t low, std::int64_t high,
                                       const Where& where)
{
    const json::Value* value = fields.take(key);
    if (value == nullptr)
    {
        return std::optional<std::int64_t>{};
    }
    const std::int64_t* number = value->as_integer();
    if (number == nullptr || *number < low || *number > high)
    {
        return fail(where.prefix() + quoted(key) + " must be " + range_text(low, high));
    }
    return std::optional<std::int64_t>{*number};
}

[[nodiscard]] Integer required_integer(Fields& fields, std::string_view key, std::int64_t low, std::int64_t high,
                                       const Where& where)
{
    auto value = optional_integer(fields, key, low, high, where);
    if (value && !*value)
    {
        return fail(where.prefix() + "it needs " + quoted(key));
    }
    return value;
}

// Text of 1 to `max` characters (bytes: the file is UTF-8 and these are for people), with no control character.
[[nodiscard]] std::expected<std::optional<std::string>, ReadError> optional_text(Fields& fields, std::string_view key,
                                                                                 std::size_t max, const Where& where)
{
    const json::Value* value = fields.take(key);
    if (value == nullptr)
    {
        return std::optional<std::string>{};
    }
    const std::string* text = value->as_string();
    if (text == nullptr || text->empty() || text->size() > max ||
        std::ranges::any_of(*text, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; }))
    {
        return fail(where.prefix() + quoted(key) + " must be text of 1 to " + std::to_string(max) +
                    " characters, with no control character");
    }
    return std::optional<std::string>{*text};
}

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

[[nodiscard]] std::expected<std::optional<unsigned>, ReadError> optional_status(Fields& fields, std::string_view key,
                                                                                const Where& where)
{
    const json::Value* value = fields.take(key);
    if (value == nullptr)
    {
        return std::optional<unsigned>{};
    }
    const std::string* text = value->as_string();
    const auto status = text != nullptr ? status_from_text(*text) : std::nullopt;
    if (!status)
    {
        return fail(where.prefix() + quoted(key) + " must be text such as \"0x1004\": 0x and four hexadecimal digits");
    }
    return std::optional<unsigned>{*status};
}

// ---- when -----------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<When, ReadError> read_when(const json::Value* value, const Where& where)
{
    if (value == nullptr)
    {
        return fail(where.prefix() + "it needs \"when\"");
    }
    const json::Object* object = value->as_object();
    if (object == nullptr)
    {
        return fail(where.prefix() + "\"when\" must be an object");
    }
    Fields fields(*object);
    When when;
    std::size_t triggers = 0;

    for (const TriggerKind kind : {TriggerKind::request, TriggerKind::status_file, TriggerKind::after_ms})
    {
        const std::string_view key = to_string(kind);
        const std::int64_t low = kind == TriggerKind::after_ms ? 0 : 1;
        const std::int64_t high = kind == TriggerKind::after_ms ? max_after_milliseconds : max_count;
        const auto number = optional_integer(fields, key, low, high, where);
        if (!number)
        {
            return std::unexpected(number.error());
        }
        if (*number)
        {
            ++triggers;
            when.kind = kind;
            when.value = static_cast<std::uint64_t>(**number);
        }
    }
    if (triggers != 1)
    {
        return fail(where.prefix() + "\"when\" needs exactly one of \"request\", \"status_file\" and \"after_ms\"");
    }

    const bool by_time = when.kind == TriggerKind::after_ms;
    const std::int64_t every_low = by_time ? min_every_ms : 1;
    const std::int64_t every_high = by_time ? max_after_milliseconds : max_every;
    const auto every = optional_integer(fields, "every", every_low, every_high, where);
    if (!every)
    {
        return std::unexpected(every.error());
    }
    if (*every)
    {
        when.every = static_cast<std::uint64_t>(**every);
    }
    const auto times = optional_integer(fields, "times", 1, max_count, where);
    if (!times)
    {
        return std::unexpected(times.error());
    }
    if (*times)
    {
        when.times = static_cast<std::uint64_t>(**times);
    }
    if (const json::Value* operation = fields.take("operation"))
    {
        const std::string* text = operation->as_string();
        const auto parsed = text != nullptr ? operation_from_string(*text) : std::nullopt;
        if (!parsed || *parsed == OperationName::any)
        {
            return fail(where.prefix() +
                        "\"operation\" must be \"information\", \"upload\", \"download_media_defined\" or "
                        "\"download_operator_defined\"");
        }
        when.operation = *parsed;
    }
    if (const auto unknown = fields.unknown())
    {
        return fail(where.prefix() + "\"when\" has the key " + quoted(*unknown) + ", which this version does not know");
    }
    return when;
}

// ---- on -------------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<Selector, ReadError> read_on(const json::Value* value, const Where& where)
{
    Selector selector;
    if (value == nullptr)
    {
        return selector;
    }
    const json::Object* object = value->as_object();
    if (object == nullptr)
    {
        return fail(where.prefix() + "\"on\" must be an object");
    }
    Fields fields(*object);
    if (const json::Value* datagram = fields.take("datagram"))
    {
        const std::string* text = datagram->as_string();
        const auto kind = text != nullptr ? datagram_kind_from_string(*text) : std::nullopt;
        if (!kind)
        {
            return fail(where.prefix() +
                        "\"datagram\" must be \"any\", \"request\", \"option_ack\", \"ack\", \"data\", \"error\" or "
                        "\"reply\"");
        }
        selector.datagram = *kind;
    }
    if (const json::Value* file = fields.take("file"))
    {
        const std::string* text = file->as_string();
        if (text == nullptr || text->empty() || text->size() > max_file_selector_chars ||
            std::ranges::any_of(*text, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; }))
        {
            return fail(where.prefix() + "\"file\" must be a file name or an extension from its dot, of 1 to " +
                        std::to_string(max_file_selector_chars) + " characters");
        }
        selector.file = *text;
    }
    const auto block = optional_integer(fields, "block", 0, 65535, where);
    if (!block)
    {
        return std::unexpected(block.error());
    }
    if (*block)
    {
        selector.block = static_cast<std::uint16_t>(**block);
    }
    if (const auto unknown = fields.unknown())
    {
        return fail(where.prefix() + "\"on\" has the key " + quoted(*unknown) + ", which this version does not know");
    }
    return selector;
}

// ---- do -------------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<Fault, ReadError> read_do(const json::Value* value, const Where& where)
{
    if (value == nullptr)
    {
        return fail(where.prefix() + "it needs \"do\"");
    }
    const json::Object* object = value->as_object();
    if (object == nullptr)
    {
        return fail(where.prefix() + "\"do\" must be an object");
    }
    Fields fields(*object);
    const json::Value* word = fields.take("action");
    if (word == nullptr)
    {
        return fail(where.prefix() + "\"do\" needs an \"action\"");
    }
    const std::string* text = word->as_string();
    const auto action = text != nullptr ? action_from_string(*text) : std::nullopt;
    if (!action)
    {
        return fail(where.prefix() + "\"action\" is " + (text != nullptr ? quoted(*text) : std::string{"not text"}) +
                    ", which this version does not know");
    }
    Fault fault;
    fault.action = *action;

    const auto set_integer = [&](std::string_view key, std::int64_t low, std::int64_t high, bool required,
                                 auto&& assign) -> std::expected<void, ReadError>
    {
        const auto number = required ? required_integer(fields, key, low, high, where)
                                     : optional_integer(fields, key, low, high, where);
        if (!number)
        {
            return std::unexpected(number.error());
        }
        if (*number)
        {
            assign(**number);
        }
        return {};
    };

    std::expected<void, ReadError> result;
    switch (*action)
    {
    case Action::drop:
    case Action::check_value:
        break;
    case Action::duplicate:
        result = set_integer("copies", 2, 16, false, [&](std::int64_t n) { fault.copies = static_cast<unsigned>(n); });
        break;
    case Action::delay:
    {
        const bool has_fixed = fields.take("ms") != nullptr;
        // `ms` was only looked for: look again for the number (taking it twice is harmless).
        if (has_fixed)
        {
            result = set_integer("ms", 1, max_delay_milliseconds, true,
                                 [&](std::int64_t n)
                                 {
                                     fault.milliseconds = n;
                                     fault.min_milliseconds = n;
                                     fault.max_milliseconds = n;
                                 });
        }
        if (result)
        {
            const bool has_range = fields.take("min_ms") != nullptr || fields.take("max_ms") != nullptr;
            if (has_fixed && has_range)
            {
                return fail(where.prefix() + "\"delay\" takes \"ms\", or \"min_ms\" and \"max_ms\", not both");
            }
            if (has_range)
            {
                std::optional<std::int64_t> low;
                std::optional<std::int64_t> high;
                result = set_integer("min_ms", 0, max_delay_milliseconds, true, [&](std::int64_t n) { low = n; });
                if (result)
                {
                    result = set_integer("max_ms", 0, max_delay_milliseconds, true, [&](std::int64_t n) { high = n; });
                }
                if (result && *low > *high)
                {
                    return fail(where.prefix() + "\"min_ms\" must not be more than \"max_ms\"");
                }
                if (result)
                {
                    fault.min_milliseconds = *low;
                    fault.max_milliseconds = *high;
                    fault.milliseconds = *low;
                }
            }
            else if (!has_fixed)
            {
                return fail(where.prefix() + "\"delay\" needs \"ms\", or \"min_ms\" and \"max_ms\"");
            }
        }
        break;
    }
    case Action::reorder:
        result =
            set_integer("within_ms", 1, max_delay_milliseconds, false, [&](std::int64_t n) { fault.milliseconds = n; });
        break;
    case Action::corrupt:
        result =
            set_integer("offset", 0, 65535, false, [&](std::int64_t n) { fault.offset = static_cast<unsigned>(n); });
        if (result)
        {
            result =
                set_integer("xor", 1, 255, false, [&](std::int64_t n) { fault.xor_mask = static_cast<unsigned>(n); });
        }
        break;
    case Action::length:
        result = set_integer("delta", -65535, 65535, true, [&](std::int64_t n) { fault.delta = static_cast<int>(n); });
        if (result && fault.delta == 0)
        {
            return fail(where.prefix() + "\"delta\" must not be 0");
        }
        break;
    case Action::error:
    {
        result = set_integer("code", 0, 65535, true, [&](std::int64_t n) { fault.code = static_cast<unsigned>(n); });
        if (result)
        {
            const auto message = optional_text(fields, "text", max_text_chars, where);
            if (!message)
            {
                return std::unexpected(message.error());
            }
            if (*message)
            {
                fault.text = **message;
            }
        }
        break;
    }
    case Action::wait:
        result =
            set_integer("seconds", 1, 65535, true, [&](std::int64_t n) { fault.seconds = static_cast<unsigned>(n); });
        break;
    case Action::abort_message:
    {
        const auto status = optional_status(fields, "status", where);
        if (!status)
        {
            return std::unexpected(status.error());
        }
        if (!*status)
        {
            return fail(where.prefix() + "it needs \"status\"");
        }
        fault.status = **status;
        break;
    }
    case Action::abort:
    {
        const auto status = optional_status(fields, "status", where);
        if (!status)
        {
            return std::unexpected(status.error());
        }
        if (*status)
        {
            if (**status != 0x1004 && **status != 0x1005)
            {
                return fail(where.prefix() + "an abort of the loader's own has \"status\" \"0x1004\" or \"0x1005\"");
            }
            fault.status = **status;
        }
        break;
    }
    case Action::silent:
        result =
            set_integer("for_ms", 1, max_after_milliseconds, false, [&](std::int64_t n) { fault.milliseconds = n; });
        break;
    }
    if (!result)
    {
        return std::unexpected(result.error());
    }
    if (const auto unknown = fields.unknown())
    {
        return fail(where.prefix() + quoted(to_string(*action)) + " does not take the key " + quoted(*unknown));
    }
    return fault;
}

// ---- a step ---------------------------------------------------------------------------------------------------

[[nodiscard]] std::expected<Step, ReadError> read_step(const json::Value& value, std::size_t number)
{
    const Where where{number};
    const json::Object* object = value.as_object();
    if (object == nullptr)
    {
        return fail(where.prefix() + "it must be an object");
    }
    Fields fields(*object);
    Step step;

    const auto comment = optional_text(fields, "comment", max_comment_chars, where);
    if (!comment)
    {
        return std::unexpected(comment.error());
    }
    if (*comment)
    {
        step.comment = **comment;
    }
    const json::Value* when_value = fields.take("when");
    const json::Value* on_value = fields.take("on");
    const json::Value* do_value = fields.take("do");

    auto when = read_when(when_value, where);
    if (!when)
    {
        return std::unexpected(when.error());
    }
    step.when = *when;
    auto fault = read_do(do_value, where);
    if (!fault)
    {
        return std::unexpected(fault.error());
    }
    step.fault = *fault;
    auto on = read_on(on_value, where);
    if (!on)
    {
        return std::unexpected(on.error());
    }
    step.on = *on;
    // A wait that does not say which datagram is the answer to a request, which is the one WAIT is for.
    const json::Object* on_object = on_value != nullptr ? on_value->as_object() : nullptr;
    const bool on_has_datagram = on_object != nullptr && std::ranges::any_of(*on_object, [](const json::Member& m)
                                                                             { return m.key == "datagram"; });

    const json::Value* count = fields.take("count");
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
            return fail(where.prefix() + "\"count\" must be " + range_text(1, max_count) + " or \"all\"");
        }
    }
    const auto per_mille = optional_integer(fields, "per_mille", 1, 1000, where);
    if (!per_mille)
    {
        return std::unexpected(per_mille.error());
    }
    if (*per_mille)
    {
        step.per_mille = static_cast<unsigned>(**per_mille);
    }
    if (const auto unknown = fields.unknown())
    {
        return fail("step " + std::to_string(number) + " has the key " + quoted(*unknown) +
                    ", which this version does not know");
    }

    // What the action goes with.
    const std::string action = quoted(to_string(step.fault.action));
    if (!acts_on_datagrams(step.fault.action))
    {
        if (on_value != nullptr || count != nullptr)
        {
            return fail(where.prefix() + action + " acts on the operation, so it takes no \"on\" or \"count\"");
        }
        if (step.when.every != 0 || step.when.times != 0)
        {
            return fail(where.prefix() + action + " happens once, so its \"when\" takes no \"every\" or \"times\"");
        }
    }
    if (step.fault.action == Action::check_value &&
        ((step.on.datagram != DatagramKind::any && step.on.datagram != DatagramKind::option_ack) || step.on.block))
    {
        return fail(where.prefix() + action +
                    " acts on an option acknowledgement, so \"on\" may give only a \"file\" (and \"datagram\" "
                    "\"option_ack\")");
    }
    if (step.fault.action == Action::wait && !on_has_datagram)
    {
        step.on.datagram = DatagramKind::reply;
    }
    return step;
}

} // namespace

std::expected<Scenario, ReadError> read_scenario(std::string_view text)
{
    if (text.size() > max_file_bytes)
    {
        return fail("the file is larger than " + std::to_string(max_file_bytes) + " bytes");
    }
    const auto parsed = json::parse_strict(text);
    if (!parsed)
    {
        return fail("it is not valid JSON, or it has a repeated key, a comment, a trailing comma, text that is not "
                    "UTF-8, or is nested deeper than " +
                    std::to_string(json::max_depth) + " levels");
    }
    const json::Object* top = parsed->as_object();
    if (top == nullptr)
    {
        return fail("it must be a JSON object");
    }
    Fields fields(*top);
    const Where where;

    const json::Value* kind = fields.take("kind");
    const std::string* kind_text = kind != nullptr ? kind->as_string() : nullptr;
    if (kind_text != nullptr && *kind_text == mock_scenario_kind)
    {
        return fail("\"kind\" is \"adl-mock-scenario\": that file is for adl_mock, and this is the loader's scenario");
    }
    if (kind_text == nullptr || *kind_text != scenario_kind)
    {
        return fail("\"kind\" must be \"adl-loader-scenario\"");
    }
    const json::Value* format = fields.take("format");
    const std::int64_t* format_number = format != nullptr ? format->as_integer() : nullptr;
    if (format_number == nullptr || *format_number < 1)
    {
        return fail("\"format\" must be 1");
    }
    if (*format_number != scenario_format)
    {
        return fail("\"format\" is " + std::to_string(*format_number) + "; this version reads " +
                    std::to_string(scenario_format) + ", so the file was made for a newer version");
    }

    Scenario scenario;
    const auto name = optional_text(fields, "name", max_name_chars, where);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    if (*name)
    {
        scenario.name = **name;
    }
    const auto description = optional_text(fields, "description", max_description_chars, where);
    if (!description)
    {
        return std::unexpected(description.error());
    }
    if (*description)
    {
        scenario.description = **description;
    }
    const auto seed = optional_integer(fields, "seed", 0, max_seed, where);
    if (!seed)
    {
        return std::unexpected(seed.error());
    }
    if (*seed)
    {
        scenario.seed = static_cast<std::uint64_t>(**seed);
    }

    const json::Value* steps = fields.take("steps");
    if (steps == nullptr)
    {
        return fail("it needs \"steps\"");
    }
    const json::Array* list = steps->as_array();
    if (list == nullptr)
    {
        return fail("\"steps\" must be a list");
    }
    if (list->size() > max_steps)
    {
        return fail("there are more than " + std::to_string(max_steps) + " steps");
    }
    if (const auto unknown = fields.unknown())
    {
        return fail("it has the key " + quoted(*unknown) + ", which this version does not know");
    }
    for (std::size_t i = 0; i < list->size(); ++i)
    {
        auto step = read_step((*list)[i], i + 1);
        if (!step)
        {
            return std::unexpected(step.error());
        }
        if (!scenario.seed)
        {
            if (step->per_mille)
            {
                return fail("step " + std::to_string(i + 1) + " uses \"per_mille\" but the scenario has no \"seed\"");
            }
            if (step->fault.action == Action::delay && step->fault.min_milliseconds < step->fault.max_milliseconds)
            {
                return fail("step " + std::to_string(i + 1) + " uses a delay range but the scenario has no \"seed\"");
            }
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
    out += "  \"kind\": " + json::quote(scenario_kind) + ",\n";
    out += "  \"format\": " + std::to_string(scenario_format) + ",\n";
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
