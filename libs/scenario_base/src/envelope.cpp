// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/scenario/envelope.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace adl::scenario
{

namespace
{

constexpr std::array<std::string_view, 3> trigger_keys{"request", "status_file", "after_ms"};

[[nodiscard]] std::unexpected<ReadError> fail(std::string message)
{
    return std::unexpected(ReadError{std::move(message)});
}

[[nodiscard]] std::string in_quotes(std::string_view key)
{
    std::string text;
    text.reserve(key.size() + 2);
    text += '"';
    text += key;
    text += '"';
    return text;
}

// A key of the file, as a sentence may quote it. A key is any text the file's author wrote: it may have a control
// character, a quote or a backslash, and it may be as long as the file, so it is shown as JSON writes it (a new line
// is `\n`) and cut to the first `max_key_characters` characters, with "..." after them.
constexpr std::size_t max_key_characters = 64;

[[nodiscard]] std::string key_as_written(std::string_view key)
{
    static constexpr std::string_view hex = "0123456789abcdef";
    std::string text;
    text += '"';
    std::size_t characters = 0;
    bool cut = false;
    for (const char c : key)
    {
        const auto byte = static_cast<unsigned char>(c);
        if ((byte & 0xC0U) != 0x80U && ++characters > max_key_characters)
        {
            cut = true;
            break;
        }
        switch (c)
        {
        case '"':
            text += "\\\"";
            break;
        case '\\':
            text += "\\\\";
            break;
        case '\n':
            text += "\\n";
            break;
        case '\r':
            text += "\\r";
            break;
        case '\t':
            text += "\\t";
            break;
        default:
            if (byte < 0x20 || byte == 0x7F)
            {
                text += "\\u00";
                text += hex[byte >> 4U];
                text += hex[byte & 0x0FU];
            }
            else
            {
                text += c;
            }
            break;
        }
    }
    if (cut)
    {
        text += "...";
    }
    text += '"';
    return text;
}

// "step 3: " for a sentence about a step, nothing for one about the top level of the file.
[[nodiscard]] std::string prefix(std::size_t step)
{
    return step == 0 ? std::string{} : "step " + std::to_string(step) + ": ";
}

[[nodiscard]] std::string from_to(std::int64_t low, std::int64_t high)
{
    return std::to_string(low) + " to " + std::to_string(high);
}

[[nodiscard]] bool has_control_character(std::string_view text) noexcept
{
    return std::ranges::any_of(text,
                               [](char c)
                               {
                                   const auto byte = static_cast<unsigned char>(c);
                                   return byte < 0x20 || byte == 0x7F;
                               });
}

[[nodiscard]] bool is_known(std::string_view key, std::span<const std::string_view> allowed) noexcept
{
    return std::ranges::find(allowed, key) != allowed.end();
}

[[nodiscard]] std::string_view whom(std::string_view kind) noexcept
{
    return kind == loader_kind ? "a loader" : "a mock target";
}

// The members of `object` whose keys the family named, as they were written.
[[nodiscard]] json::Object members_named(const json::Object& object, std::span<const std::string_view> keys)
{
    json::Object found;
    for (const json::Member& member : object)
    {
        if (is_known(member.key, keys))
        {
            found.push_back(member);
        }
    }
    return found;
}

[[nodiscard]] std::vector<std::string_view> joined(std::initializer_list<std::string_view> shared,
                                                   std::span<const std::string_view> more)
{
    std::vector<std::string_view> keys(shared);
    keys.insert(keys.end(), more.begin(), more.end());
    return keys;
}

[[nodiscard]] std::expected<Step, ReadError> read_step(const json::Value& value, std::size_t number,
                                                       const Family& family, bool has_seed)
{
    const json::Object* step_object = value.as_object();
    if (step_object == nullptr)
    {
        return fail("step " + std::to_string(number) + " must be an object");
    }
    const std::vector<std::string_view> step_keys = joined({"when", "do", "per_mille", "comment"}, family.step_keys);
    if (const auto error = only_keys(*step_object, step_keys, number))
    {
        return std::unexpected(*error);
    }

    Step step;
    step.number = number;

    // when
    const json::Value* when_value = json::find(*step_object, "when");
    if (when_value == nullptr)
    {
        return fail(prefix(number) + "\"when\" is missing; it says when the step happens");
    }
    const json::Object* when = when_value->as_object();
    if (when == nullptr)
    {
        return fail(prefix(number) + "\"when\" must be an object");
    }
    std::size_t triggers = 0;
    Trigger trigger = Trigger::request;
    for (std::size_t i = 0; i < trigger_keys.size(); ++i)
    {
        if (json::find(*when, trigger_keys[i]) != nullptr)
        {
            ++triggers;
            trigger = static_cast<Trigger>(i);
        }
    }
    if (triggers != 1)
    {
        return fail(prefix(number) + "\"when\" needs exactly one of \"request\", \"status_file\" and \"after_ms\"");
    }
    const std::vector<std::string_view> when_keys =
        joined({"request", "status_file", "after_ms", "every", "times"}, family.when_keys);
    if (const auto error = only_keys(*when, when_keys, number, "when"))
    {
        return std::unexpected(*error);
    }
    step.when.trigger = trigger;
    const bool is_time = trigger == Trigger::after_ms;
    const auto at =
        whole_number(*when, trigger_name(trigger), is_time ? 0 : 1, is_time ? max_after_ms : max_occurrence, number);
    if (!at)
    {
        return std::unexpected(at.error());
    }
    step.when.at = *at;
    const auto every = optional_whole_number(*when, "every", is_time ? min_every_ms : 1,
                                             is_time ? max_after_ms : max_occurrence, number);
    if (!every)
    {
        return std::unexpected(every.error());
    }
    step.when.every = *every;
    const auto times = optional_whole_number(*when, "times", 1, max_occurrence, number);
    if (!times)
    {
        return std::unexpected(times.error());
    }
    step.when.times = *times;
    step.when.rest = members_named(*when, family.when_keys);

    // per_mille and comment
    const auto per_mille = optional_whole_number(*step_object, "per_mille", 1, max_per_mille, number);
    if (!per_mille)
    {
        return std::unexpected(per_mille.error());
    }
    step.per_mille = *per_mille;
    if (step.per_mille && !has_seed)
    {
        return std::unexpected(missing_seed(number, "per_mille"));
    }
    const auto comment = optional_text(*step_object, "comment", max_comment_characters, number);
    if (!comment)
    {
        return std::unexpected(comment.error());
    }
    step.comment = comment->value_or(std::string{});

    // do
    const json::Value* do_value = json::find(*step_object, "do");
    if (do_value == nullptr)
    {
        return fail(prefix(number) + "\"do\" is missing; it says what the step does");
    }
    const json::Object* action = do_value->as_object();
    if (action == nullptr)
    {
        return fail(prefix(number) + "\"do\" must be an object");
    }
    const auto action_word = text(*action, "action", max_action_characters, number);
    if (!action_word)
    {
        return std::unexpected(action_word.error());
    }
    if (!json::is_token(*action_word, max_action_characters, "_"))
    {
        return fail(prefix(number) + "\"action\" must be a word of lower-case letters, digits and \"_\"");
    }
    step.action = *action_word;
    step.action_members = *action;
    step.rest = members_named(*step_object, family.step_keys);
    return step;
}

} // namespace

std::string_view trigger_name(Trigger trigger) noexcept
{
    return trigger_keys[static_cast<std::size_t>(trigger)];
}

std::size_t character_count(std::string_view utf8) noexcept
{
    return static_cast<std::size_t>(
        std::ranges::count_if(utf8, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; }));
}

std::expected<std::int64_t, ReadError> whole_number(const json::Object& object, std::string_view key, std::int64_t low,
                                                    std::int64_t high, std::size_t step)
{
    const json::Value* value = json::find(object, key);
    if (value == nullptr)
    {
        return fail(prefix(step) + in_quotes(key) + " is missing; it is a whole number from " + from_to(low, high));
    }
    const std::int64_t* number = value->as_integer();
    if (number == nullptr || *number < low || *number > high)
    {
        return fail(prefix(step) + in_quotes(key) + " must be a whole number from " + from_to(low, high));
    }
    return *number;
}

std::expected<std::optional<std::int64_t>, ReadError> optional_whole_number(const json::Object& object,
                                                                            std::string_view key, std::int64_t low,
                                                                            std::int64_t high, std::size_t step)
{
    if (json::find(object, key) == nullptr)
    {
        return std::optional<std::int64_t>{};
    }
    const auto number = whole_number(object, key, low, high, step);
    if (!number)
    {
        return std::unexpected(number.error());
    }
    return std::optional<std::int64_t>{*number};
}

std::expected<std::string, ReadError> text(const json::Object& object, std::string_view key, std::size_t max_characters,
                                           std::size_t step)
{
    const std::string what = "text of 1 to " + std::to_string(max_characters) + " characters";
    const json::Value* value = json::find(object, key);
    if (value == nullptr)
    {
        return fail(prefix(step) + in_quotes(key) + " is missing; it is " + what);
    }
    const std::string* content = value->as_string();
    if (content == nullptr || content->empty() || character_count(*content) > max_characters ||
        has_control_character(*content))
    {
        return fail(prefix(step) + in_quotes(key) + " must be " + what + ", with no control character");
    }
    return *content;
}

std::expected<std::optional<std::string>, ReadError> optional_text(const json::Object& object, std::string_view key,
                                                                   std::size_t max_characters, std::size_t step)
{
    if (json::find(object, key) == nullptr)
    {
        return std::optional<std::string>{};
    }
    auto content = text(object, key, max_characters, step);
    if (!content)
    {
        return std::unexpected(content.error());
    }
    return std::optional<std::string>{std::move(*content)};
}

std::optional<ReadError> only_keys(const json::Object& object, std::span<const std::string_view> allowed,
                                   std::size_t step, std::string_view part)
{
    for (const json::Member& member : object)
    {
        if (is_known(member.key, allowed))
        {
            continue;
        }
        const std::string unknown = "the key " + key_as_written(member.key) + ", which this version does not know";
        if (step == 0)
        {
            return ReadError{"the file has " + unknown};
        }
        if (part.empty())
        {
            return ReadError{"step " + std::to_string(step) + " has " + unknown};
        }
        return ReadError{prefix(step) + in_quotes(part) + " has " + unknown};
    }
    return std::nullopt;
}

ReadError missing_seed(std::size_t step, std::string_view key)
{
    return ReadError{"step " + std::to_string(step) + " uses " + in_quotes(key) + " but the scenario has no \"seed\""};
}

ReadError not_taken(std::size_t step, std::string_view key, std::string_view what)
{
    return ReadError{prefix(step) + in_quotes(key) + " must be " + std::string{what}};
}

std::expected<Envelope, ReadError> read_envelope(std::string_view text_of_file, const Family& family)
{
    if (text_of_file.size() > max_file_size)
    {
        return fail("the file is larger than " + std::to_string(max_file_size) + " bytes");
    }
    const auto document = json::parse_strict(text_of_file);
    if (!document)
    {
        return fail("it is not valid JSON, or it has a repeated key, a comment, a trailing comma, text that is not "
                    "UTF-8, or is nested deeper than " +
                    std::to_string(json::max_depth) + " levels");
    }
    const json::Object* top = document->as_object();
    if (top == nullptr)
    {
        return fail("the file must be one JSON object");
    }

    // kind and format come first, so that a file made for another kind of target or for a newer version says so
    // and is not reported for the keys it has that this version does not know.
    const json::Value* kind = json::find(*top, "kind");
    if (kind == nullptr)
    {
        return fail("\"kind\" is missing; it must be " + in_quotes(family.kind));
    }
    const std::string* kind_text = kind->as_string();
    if (kind_text == nullptr || *kind_text != family.kind)
    {
        if (kind_text != nullptr && (*kind_text == mock_kind || *kind_text == loader_kind))
        {
            return fail("this is a scenario for " + std::string{whom(*kind_text)} + ", not for " +
                        std::string{whom(family.kind)} + " (\"kind\" is " + in_quotes(*kind_text) + ")");
        }
        return fail("\"kind\" must be " + in_quotes(family.kind));
    }
    const json::Value* format = json::find(*top, "format");
    if (format == nullptr)
    {
        return fail("\"format\" is missing; it must be " + std::to_string(format_number));
    }
    const std::int64_t* format_value = format->as_integer();
    if (format_value != nullptr && *format_value > format_number)
    {
        return fail("\"format\" is " + std::to_string(*format_value) + "; this version reads " +
                    std::to_string(format_number) + ", so the file was made for a newer version");
    }
    if (format_value == nullptr || *format_value != format_number)
    {
        return fail("\"format\" must be " + std::to_string(format_number));
    }
    static constexpr std::array<std::string_view, 6> top_keys{"kind", "format", "name", "description", "seed", "steps"};
    if (const auto error = only_keys(*top, top_keys, 0))
    {
        return std::unexpected(*error);
    }

    Envelope envelope;
    const auto name = optional_text(*top, "name", max_name_characters, 0);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    envelope.name = name->value_or(std::string{});
    const auto description = optional_text(*top, "description", max_description_characters, 0);
    if (!description)
    {
        return std::unexpected(description.error());
    }
    envelope.description = description->value_or(std::string{});
    const auto seed = optional_whole_number(*top, "seed", 0, max_seed, 0);
    if (!seed)
    {
        return std::unexpected(seed.error());
    }
    if (*seed)
    {
        envelope.seed = static_cast<std::uint64_t>(**seed);
    }

    const json::Value* steps = json::find(*top, "steps");
    if (steps == nullptr)
    {
        return fail("\"steps\" is missing; it is a list of steps");
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
    envelope.steps.reserve(list->size());
    for (std::size_t i = 0; i < list->size(); ++i)
    {
        auto step = read_step((*list)[i], i + 1, family, envelope.seed.has_value());
        if (!step)
        {
            return std::unexpected(step.error());
        }
        envelope.steps.push_back(std::move(*step));
    }
    return envelope;
}

} // namespace adl::scenario
