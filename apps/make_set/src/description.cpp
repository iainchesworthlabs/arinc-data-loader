// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/make_set/description.hpp"

#include "adl/licence/json.hpp"
#include "adl/make_set/json.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "adl/set_maker/validate.hpp"

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace adl::make_set
{

namespace
{

namespace json = adl::licence::json;
using adl::arinc665::CheckValueType;
using adl::set_maker::FileKind;

constexpr std::string_view kind_text = "adl-media-set-description";
constexpr std::string_view check_words = "none, crc8, crc16, crc32, md5, sha1";
constexpr std::size_t most_members = 255;
constexpr std::int64_t most_type_id = 65535;

template <typename T>
using Read = std::expected<T, std::string>;

std::unexpected<std::string> fail(std::string sentence)
{
    return std::unexpected{std::move(sentence)};
}

std::string index_of(std::string_view list, std::size_t index)
{
    return std::string{list} + "[" + std::to_string(index) + "]";
}

std::string join(std::string_view where, std::string_view key)
{
    return where.empty() ? std::string{key} : std::string{where} + "." + std::string{key};
}

// An object of the document, with the place it was found.
struct Fields
{
    const json::Object* object = nullptr;
    std::string where;

    [[nodiscard]] std::string at(std::string_view key) const
    {
        return join(where, key);
    }
    // The member, or null when it is absent or a JSON null: for a key that may be left out.
    [[nodiscard]] const json::Value* get(std::string_view key) const noexcept
    {
        const json::Value* value = json::find(*object, key);
        return value == nullptr || value->is_null() ? nullptr : value;
    }
};

Read<Fields> fields_of(const json::Value& value, std::string where)
{
    const json::Object* object = value.as_object();
    if (object == nullptr)
    {
        return fail(where + ": expected an object.");
    }
    return Fields{object, std::move(where)};
}

// Refuses a key that is not in `known`.
std::string unknown_key(const Fields& fields, std::initializer_list<std::string_view> known)
{
    for (const json::Member& member : *fields.object)
    {
        if (std::ranges::find(known, std::string_view{member.key}) == known.end())
        {
            return fields.at(member.key) + ": this key is not known.";
        }
    }
    return {};
}

bool printable_ascii(std::string_view text) noexcept
{
    return std::ranges::all_of(text, [](char c) { return c >= 0x20 && c <= 0x7E; });
}

Read<std::optional<std::string>> optional_text(const Fields& fields, std::string_view key, std::size_t most)
{
    const json::Value* value = fields.get(key);
    if (value == nullptr)
    {
        return std::optional<std::string>{};
    }
    const std::string* text = value->as_string();
    if (text == nullptr || text->empty() || text->size() > most)
    {
        return fail(fields.at(key) + ": expected a string of 1 to " + std::to_string(most) + " bytes.");
    }
    return std::optional<std::string>{*text};
}

Read<std::string> required_text(const Fields& fields, std::string_view key, std::size_t most)
{
    auto text = optional_text(fields, key, most);
    if (!text)
    {
        return std::unexpected{text.error()};
    }
    if (!*text)
    {
        return fail(fields.at(key) + ": this key is missing.");
    }
    return std::move(**text);
}

Read<std::optional<std::int64_t>> optional_integer(const Fields& fields, std::string_view key, std::int64_t least,
                                                   std::int64_t most)
{
    const json::Value* value = fields.get(key);
    if (value == nullptr)
    {
        return std::optional<std::int64_t>{};
    }
    const std::int64_t* number = value->as_integer();
    if (number == nullptr || *number < least || *number > most)
    {
        return fail(fields.at(key) + ": expected a whole number from " + std::to_string(least) + " to " +
                    std::to_string(most) + ".");
    }
    return std::optional<std::int64_t>{*number};
}

// A list that has to be there with `least` to `most` entries, or one that may be left out.
Read<const json::Array*> list_of(const Fields& fields, std::string_view key, std::size_t least, std::size_t most,
                                 bool required)
{
    const json::Value* value = fields.get(key);
    if (value == nullptr)
    {
        if (required)
        {
            return fail(fields.at(key) + ": this key is missing.");
        }
        return static_cast<const json::Array*>(nullptr);
    }
    const json::Array* items = value->as_array();
    if (items == nullptr)
    {
        return fail(fields.at(key) + ": expected a list.");
    }
    if (items->size() < least || items->size() > most)
    {
        return fail(fields.at(key) + ": expected a list of " + std::to_string(least) + " to " + std::to_string(most) +
                    " entries.");
    }
    return items;
}

Read<CheckValueType> check_of(const Fields& fields, std::string_view key)
{
    auto word = optional_text(fields, key, max_text_bytes);
    if (!word)
    {
        return fail(fields.at(key) + ": expected one of " + std::string{check_words} + ".");
    }
    if (!*word)
    {
        return CheckValueType::none;
    }
    const auto type = adl::set_maker::check_type_from_word(**word);
    if (!type)
    {
        return fail(fields.at(key) + ": expected one of " + std::string{check_words} + ".");
    }
    return *type;
}

// A relative path written with /, which means the same on every system. Empty when it is one.
std::string source_problem(const Fields& fields, std::string_view source)
{
    bool good = !source.empty() && source.front() != '/' && source.back() != '/';
    good = good &&
           std::ranges::none_of(source, [](char c)
                                { return c == '\\' || c == ':' || static_cast<unsigned char>(c) < 0x20 || c == 0x7F; });
    for (std::size_t start = 0; good && start <= source.size();)
    {
        const std::size_t end = std::min(source.find('/', start), source.size());
        const std::string_view part = source.substr(start, end - start);
        good = !part.empty() && part != ".";
        start = end + 1;
    }
    return good ? std::string{} : fields.at("source") + ": this source path is not a relative path written with /.";
}

std::string last_part(std::string_view source)
{
    const std::size_t slash = source.rfind('/');
    return std::string{slash == std::string_view::npos ? source : source.substr(slash + 1)};
}

Read<std::optional<adl::arinc665::LoadType>> type_of(const Fields& load)
{
    const json::Value* value = load.get("type");
    if (value == nullptr)
    {
        return std::optional<adl::arinc665::LoadType>{};
    }
    const std::string where = load.at("type");
    if (const std::string* code = value->as_string())
    {
        if (auto type = adl::set_maker::load_type_from_code(*code))
        {
            return std::optional<adl::arinc665::LoadType>{std::move(*type)};
        }
        return fail(where + ": expected one of OPS, NDB, ADB, TST, or an object with description and id.");
    }
    auto object = fields_of(*value, where);
    if (!object)
    {
        return fail(where + ": expected one of OPS, NDB, ADB, TST, or an object with description and id.");
    }
    if (std::string problem = unknown_key(*object, {"description", "id"}); !problem.empty())
    {
        return fail(std::move(problem));
    }
    auto description = required_text(*object, "description", max_text_bytes);
    if (!description)
    {
        return std::unexpected{description.error()};
    }
    if (!printable_ascii(*description))
    {
        return fail(object->at("description") + ": expected printable ASCII characters.");
    }
    auto id = optional_integer(*object, "id", 0, most_type_id);
    if (!id)
    {
        return std::unexpected{id.error()};
    }
    if (!*id)
    {
        return fail(object->at("id") + ": this key is missing.");
    }
    return std::optional<adl::arinc665::LoadType>{
        adl::arinc665::LoadType{std::move(*description), static_cast<std::uint16_t>(**id)}};
}

Read<adl::set_maker::PlannedFile> file_of(const json::Value& value, std::string where,
                                          const std::filesystem::path& root, std::string& source_text)
{
    auto file = fields_of(value, std::move(where));
    if (!file)
    {
        return std::unexpected{file.error()};
    }
    if (std::string problem = unknown_key(*file, {"source", "name", "kind", "check"}); !problem.empty())
    {
        return fail(std::move(problem));
    }
    auto source = required_text(*file, "source", max_source_bytes);
    if (!source)
    {
        return std::unexpected{source.error()};
    }
    if (std::string problem = source_problem(*file, *source); !problem.empty())
    {
        return fail(std::move(problem));
    }
    auto name = optional_text(*file, "name", max_text_bytes);
    if (!name)
    {
        return std::unexpected{name.error()};
    }
    auto kind = optional_text(*file, "kind", max_text_bytes);
    if (!kind)
    {
        return std::unexpected{kind.error()};
    }
    if (*kind && **kind != "data" && **kind != "support")
    {
        return fail(file->at("kind") + ": expected one of data, support.");
    }
    auto check = check_of(*file, "check");
    if (!check)
    {
        return std::unexpected{check.error()};
    }

    adl::set_maker::PlannedFile planned;
    source_text = *source;
    planned.source = (root / adl::platform::path_from_utf8(*source)).make_preferred();
    planned.name = *name ? std::move(**name) : last_part(*source);
    planned.kind = *kind && **kind == "support" ? FileKind::support : FileKind::data;
    planned.check = *check;
    return planned;
}

Read<adl::set_maker::PlannedLoad> load_of(const json::Value& value, std::string where,
                                          const std::filesystem::path& root, std::vector<std::string>& sources,
                                          std::size_t& files_so_far)
{
    auto load = fields_of(value, std::move(where));
    if (!load)
    {
        return std::unexpected{load.error()};
    }
    if (std::string problem =
            unknown_key(*load, {"part_number", "header_file", "type", "target_hardware_ids", "check", "files"});
        !problem.empty())
    {
        return fail(std::move(problem));
    }
    adl::set_maker::PlannedLoad planned;
    auto part_number = required_text(*load, "part_number", max_text_bytes);
    if (!part_number)
    {
        return std::unexpected{part_number.error()};
    }
    planned.part_number = std::move(*part_number);
    auto header = optional_text(*load, "header_file", max_text_bytes);
    if (!header)
    {
        return std::unexpected{header.error()};
    }
    planned.header_file_name =
        *header ? std::move(**header) : adl::set_maker::default_header_file_name(planned.part_number);
    auto type = type_of(*load);
    if (!type)
    {
        return std::unexpected{type.error()};
    }
    planned.type = std::move(*type);
    auto ids = list_of(*load, "target_hardware_ids", 0, max_target_hardware_ids, false);
    if (!ids)
    {
        return std::unexpected{ids.error()};
    }
    if (*ids != nullptr)
    {
        for (std::size_t i = 0; i < (*ids)->size(); ++i)
        {
            const std::string* id = (**ids)[i].as_string();
            if (id == nullptr || id->empty() || id->size() > max_text_bytes)
            {
                return fail(index_of(load->at("target_hardware_ids"), i) + ": expected a string of 1 to " +
                            std::to_string(max_text_bytes) + " bytes.");
            }
            planned.target_hardware_ids.push_back(*id);
        }
    }
    auto check = check_of(*load, "check");
    if (!check)
    {
        return std::unexpected{check.error()};
    }
    planned.check = *check;
    auto files = list_of(*load, "files", 1, max_files_in_a_load, true);
    if (!files)
    {
        return std::unexpected{files.error()};
    }
    for (std::size_t i = 0; i < (*files)->size(); ++i)
    {
        std::string source;
        auto file = file_of((**files)[i], index_of(load->at("files"), i), root, source);
        if (!file)
        {
            return std::unexpected{file.error()};
        }
        planned.files.push_back(std::move(*file));
        sources.push_back(std::move(source));
    }
    files_so_far += (*files)->size();
    return planned;
}

Read<adl::set_maker::PlannedBatch> batch_of(const json::Value& value, const adl::set_maker::SetPlan& plan)
{
    auto batch = fields_of(value, "batch");
    if (!batch)
    {
        return std::unexpected{batch.error()};
    }
    if (std::string problem = unknown_key(*batch, {"part_number", "file_name", "comment", "targets"}); !problem.empty())
    {
        return fail(std::move(problem));
    }
    adl::set_maker::PlannedBatch planned;
    auto part_number = required_text(*batch, "part_number", max_text_bytes);
    if (!part_number)
    {
        return std::unexpected{part_number.error()};
    }
    planned.part_number = std::move(*part_number);
    auto file_name = optional_text(*batch, "file_name", max_text_bytes);
    if (!file_name)
    {
        return std::unexpected{file_name.error()};
    }
    planned.file_name =
        *file_name ? std::move(**file_name) : adl::set_maker::default_batch_file_name(planned.part_number);
    auto comment = optional_text(*batch, "comment", max_text_bytes);
    if (!comment)
    {
        return std::unexpected{comment.error()};
    }
    if (*comment)
    {
        if (!printable_ascii(**comment))
        {
            return fail(batch->at("comment") + ": expected printable ASCII characters.");
        }
        planned.comment = std::move(**comment);
    }
    auto targets = list_of(*batch, "targets", 1, max_batch_targets, false);
    if (!targets)
    {
        return std::unexpected{targets.error()};
    }
    if (*targets == nullptr)
    {
        planned.targets = adl::set_maker::derive_batch_targets(plan.loads);
        return planned;
    }
    for (std::size_t i = 0; i < (*targets)->size(); ++i)
    {
        auto target = fields_of((**targets)[i], index_of(batch->at("targets"), i));
        if (!target)
        {
            return std::unexpected{target.error()};
        }
        if (std::string problem = unknown_key(*target, {"id", "loads"}); !problem.empty())
        {
            return fail(std::move(problem));
        }
        adl::set_maker::PlannedBatchTarget planned_target;
        auto id = required_text(*target, "id", max_text_bytes);
        if (!id)
        {
            return std::unexpected{id.error()};
        }
        planned_target.id = std::move(*id);
        auto loads = list_of(*target, "loads", 1, max_loads_in_a_batch_target, true);
        if (!loads)
        {
            return std::unexpected{loads.error()};
        }
        for (std::size_t k = 0; k < (*loads)->size(); ++k)
        {
            const std::string* part = (**loads)[k].as_string();
            if (part == nullptr || part->empty() || part->size() > max_text_bytes)
            {
                return fail(index_of(target->at("loads"), k) + ": expected a string of 1 to " +
                            std::to_string(max_text_bytes) + " bytes.");
            }
            planned_target.load_part_numbers.push_back(*part);
        }
        planned.targets.push_back(std::move(planned_target));
    }
    return planned;
}

Read<Description> read_document(const json::Value& document, const std::filesystem::path& root)
{
    auto top = fields_of(document, "");
    if (!top)
    {
        return fail("The description is not an object.");
    }
    if (std::string problem = unknown_key(*top, {"format", "kind", "media_set", "loads", "batch"}); !problem.empty())
    {
        return fail(std::move(problem));
    }

    const json::Value* format = json::find(*top->object, "format");
    if (format == nullptr)
    {
        return fail("format: this key is missing.");
    }
    if (format->as_integer() == nullptr)
    {
        return fail("format: expected a whole number.");
    }
    if (*format->as_integer() != 1)
    {
        return fail("format " + std::to_string(*format->as_integer()) +
                    " is not one this program reads (it reads format 1).");
    }
    auto kind = required_text(*top, "kind", max_text_bytes);
    if (!kind)
    {
        return std::unexpected{kind.error()};
    }
    if (*kind != kind_text)
    {
        return fail("kind: expected one of " + std::string{kind_text} + ".");
    }

    Description description;
    const json::Value* media_set_value = top->get("media_set");
    if (media_set_value == nullptr)
    {
        return fail("media_set: this key is missing.");
    }
    auto media_set = fields_of(*media_set_value, "media_set");
    if (!media_set)
    {
        return std::unexpected{media_set.error()};
    }
    if (std::string problem = unknown_key(*media_set, {"part_number", "members", "files_check", "files_list_check"});
        !problem.empty())
    {
        return fail(std::move(problem));
    }
    adl::set_maker::SetPlan& plan = description.plan;
    auto part_number = required_text(*media_set, "part_number", max_text_bytes);
    if (!part_number)
    {
        return std::unexpected{part_number.error()};
    }
    plan.part_number = std::move(*part_number);
    auto members = optional_integer(*media_set, "members", 1, static_cast<std::int64_t>(most_members));
    if (!members)
    {
        return std::unexpected{members.error()};
    }
    plan.members = *members ? static_cast<unsigned>(**members) : 1U;
    auto files_check = check_of(*media_set, "files_check");
    if (!files_check)
    {
        return std::unexpected{files_check.error()};
    }
    plan.files_check = *files_check;
    auto files_list_check = check_of(*media_set, "files_list_check");
    if (!files_list_check)
    {
        return std::unexpected{files_list_check.error()};
    }
    plan.files_list_check = *files_list_check;

    auto loads = list_of(*top, "loads", 1, max_loads, true);
    if (!loads)
    {
        return std::unexpected{loads.error()};
    }
    std::size_t files_in_all = 0;
    for (std::size_t i = 0; i < (*loads)->size(); ++i)
    {
        std::vector<std::string> sources;
        auto load = load_of((**loads)[i], index_of("loads", i), root, sources, files_in_all);
        if (!load)
        {
            return std::unexpected{load.error()};
        }
        plan.loads.push_back(std::move(*load));
        description.sources.push_back(std::move(sources));
        if (files_in_all > max_files_in_all)
        {
            return fail("More than " + std::to_string(max_files_in_all) + " files in all.");
        }
    }

    if (const json::Value* batch = top->get("batch"))
    {
        auto planned = batch_of(*batch, plan);
        if (!planned)
        {
            return std::unexpected{planned.error()};
        }
        plan.batch = std::move(*planned);
    }
    return description;
}

} // namespace

std::expected<Description, DescriptionError> read_description(std::string_view text,
                                                              const std::filesystem::path& source_root)
{
    if (text.size() > max_description_bytes)
    {
        return std::unexpected{DescriptionError{{"The text is larger than 4 MiB."}}};
    }
    auto document = json::parse_strict(text);
    if (!document)
    {
        return std::unexpected{DescriptionError{
            {"The text is not JSON that this program reads: a key given twice, a comment, a trailing comma, a "
             "byte order mark, text after the value, a control character in a string, bytes that are not UTF-8 "
             "or a value nested more than eight deep are refused."}}};
    }
    auto description = read_document(*document, source_root);
    if (!description)
    {
        return std::unexpected{DescriptionError{{std::move(description.error())}}};
    }
    DescriptionError error;
    for (const adl::set_maker::Problem& problem : adl::set_maker::validate(description->plan))
    {
        error.sentences.push_back(problem.where + ": " + problem.text + ".");
    }
    if (!error.sentences.empty())
    {
        return std::unexpected{std::move(error)};
    }
    return std::move(*description);
}

std::expected<Description, DescriptionError>
read_description_file(const std::filesystem::path& file, const std::optional<std::filesystem::path>& source_root)
{
    const std::string name = adl::platform::to_utf8(file.u8string());
    std::error_code error;
    const auto size = std::filesystem::file_size(file, error);
    if (error)
    {
        return std::unexpected{DescriptionError{{"Cannot read " + name + "."}}};
    }
    if (size > max_description_bytes)
    {
        return std::unexpected{DescriptionError{{name + " is larger than 4 MiB."}}};
    }
    std::ifstream stream(file, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!stream || !stream.read(text.data(), static_cast<std::streamsize>(size)))
    {
        return std::unexpected{DescriptionError{{"Cannot read " + name + "."}}};
    }
    return read_description(text, source_root ? *source_root : file.parent_path());
}

std::string write_description(const Description& description)
{
    const adl::set_maker::SetPlan& plan = description.plan;
    JsonWriter out;
    out.begin_object();
    out.member_int("format", 1).member("kind", kind_text);
    out.key("media_set").begin_object();
    out.member("part_number", plan.part_number).member_int("members", plan.members);
    out.member("files_check", adl::set_maker::check_type_word(plan.files_check));
    out.member("files_list_check", adl::set_maker::check_type_word(plan.files_list_check));
    out.end_object();
    out.key("loads").begin_array();
    for (std::size_t i = 0; i < plan.loads.size(); ++i)
    {
        const adl::set_maker::PlannedLoad& load = plan.loads[i];
        out.begin_object();
        out.member("part_number", load.part_number).member("header_file", load.header_file_name);
        if (load.type)
        {
            out.key("type").begin_object().member("description", load.type->description);
            out.member_int("id", load.type->id).end_object();
        }
        else
        {
            out.member_null("type");
        }
        out.member("target_hardware_ids", load.target_hardware_ids);
        out.member("check", adl::set_maker::check_type_word(load.check));
        out.key("files").begin_array();
        for (std::size_t k = 0; k < load.files.size(); ++k)
        {
            const adl::set_maker::PlannedFile& file = load.files[k];
            out.begin_object();
            out.member("source", description.sources[i][k]).member("name", file.name);
            out.member("kind", file.kind == FileKind::data ? "data" : "support");
            out.member("check", adl::set_maker::check_type_word(file.check));
            out.end_object();
        }
        out.end_array().end_object();
    }
    out.end_array();
    if (plan.batch)
    {
        out.key("batch").begin_object();
        out.member("part_number", plan.batch->part_number).member("file_name", plan.batch->file_name);
        if (plan.batch->comment.empty())
        {
            out.member_null("comment");
        }
        else
        {
            out.member("comment", plan.batch->comment);
        }
        out.key("targets").begin_array();
        for (const adl::set_maker::PlannedBatchTarget& target : plan.batch->targets)
        {
            out.begin_object().member("id", target.id).member("loads", target.load_part_numbers).end_object();
        }
        out.end_array().end_object();
    }
    else
    {
        out.member_null("batch");
    }
    out.end_object();
    return out.text();
}

} // namespace adl::make_set
