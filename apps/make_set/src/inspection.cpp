// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/make_set/inspection.hpp"

#include "adl/make_set/json.hpp"
#include "adl/make_set/version.hpp"

#include <optional>

namespace adl::make_set
{

namespace
{

using adl::set_maker::Inspection;

void optional_text(JsonWriter& out, std::string_view name, const std::optional<std::string>& value)
{
    if (value)
    {
        out.member(name, *value);
    }
    else
    {
        out.member_null(name);
    }
}

void load_files(JsonWriter& out, std::string_view name, const std::vector<adl::set_maker::InspectedLoadFile>& files)
{
    out.key(name).begin_array();
    for (const adl::set_maker::InspectedLoadFile& file : files)
    {
        out.begin_object();
        out.member("name", file.name).member("part_number", file.part_number);
        out.member_int("size", static_cast<std::int64_t>(file.size)).member("crc16", file.crc16);
        out.member("check_type", file.check_type);
        optional_text(out, "check_value", file.check_value);
        out.member_int("member", file.member).member("pathname", file.pathname);
        out.end_object();
    }
    out.end_array();
}

void media_set(JsonWriter& out, const Inspection& inspection)
{
    out.key("media_set").begin_object();
    out.member("part_number", inspection.part_number).member_int("members", inspection.members);
    out.key("members_present").begin_array();
    for (const unsigned member : inspection.members_present)
    {
        out.integer(member);
    }
    out.end_array();
    out.key("format_versions").begin_object();
    out.member("loads_list", inspection.loads_list_version).member("files_list", inspection.files_list_version);
    optional_text(out, "batches_list", inspection.batches_list_version);
    out.end_object();
    out.key("user_defined_data").begin_object();
    out.member("loads_list", inspection.loads_list_user_data).member("files_list", inspection.files_list_user_data);
    optional_text(out, "batches_list", inspection.batches_list_user_data);
    out.end_object();
    out.member("files_list_check_type", inspection.files_list_check_type);
    optional_text(out, "files_list_check_value", inspection.files_list_check_value);
    out.key("member_sizes").begin_array();
    for (const adl::set_maker::MemberSize& size : inspection.member_sizes)
    {
        out.begin_object();
        out.member_int("member", size.member).member_int("files", static_cast<std::int64_t>(size.files));
        out.member_int("size", static_cast<std::int64_t>(size.size));
        out.end_object();
    }
    out.end_array();
    out.end_object();
}

void loads(JsonWriter& out, const Inspection& inspection)
{
    out.key("loads").begin_array();
    for (const adl::set_maker::InspectedLoad& load : inspection.loads)
    {
        out.begin_object();
        out.member("part_number", load.part_number).member("header_file", load.header_file);
        out.member_int("header_member", load.header_member).member("header_pathname", load.header_pathname);
        out.member("format_version", load.format_version);
        if (load.type)
        {
            out.key("type").begin_object().member("description", load.type->description);
            out.member_int("id", load.type->id).end_object();
        }
        else
        {
            out.member_null("type");
        }
        out.member_bool("is_download", load.is_download);
        out.member("target_hardware_ids", load.target_hardware_ids);
        out.key("target_hardware_ids_with_positions").begin_array();
        for (const adl::set_maker::InspectedPositions& positions : load.target_hardware_ids_with_positions)
        {
            out.begin_object().member("id", positions.id).member("positions", positions.positions).end_object();
        }
        out.end_array();
        out.member("check_type", load.check_type);
        optional_text(out, "check_value", load.check_value);
        out.member("load_crc", load.load_crc).member("user_defined_data", load.user_defined_data);
        load_files(out, "data_files", load.data_files);
        load_files(out, "support_files", load.support_files);
        out.end_object();
    }
    out.end_array();
}

void batches(JsonWriter& out, const Inspection& inspection)
{
    out.key("batches").begin_array();
    for (const adl::set_maker::InspectedBatch& batch : inspection.batches)
    {
        out.begin_object();
        out.member("part_number", batch.part_number).member("file_name", batch.file_name);
        out.member_int("member", batch.member).member("pathname", batch.pathname);
        out.member("format_version", batch.format_version).member("comment", batch.comment);
        out.key("targets").begin_array();
        for (const adl::set_maker::InspectedBatchTarget& target : batch.targets)
        {
            out.begin_object().member("target_hardware_id_position", target.target_hardware_id_position);
            out.key("loads").begin_array();
            for (const adl::set_maker::InspectedBatchLoad& load : target.loads)
            {
                out.begin_object()
                    .member("header_file_name", load.header_file_name)
                    .member("load_part_number", load.load_part_number)
                    .end_object();
            }
            out.end_array().end_object();
        }
        out.end_array().end_object();
    }
    out.end_array();
}

void files(JsonWriter& out, const Inspection& inspection)
{
    out.key("files").begin_array();
    for (const adl::set_maker::InspectedFile& file : inspection.files)
    {
        out.begin_object();
        out.member("name", file.name).member("pathname", file.pathname).member_int("member", file.member);
        out.member_int("size", static_cast<std::int64_t>(file.size)).member("crc16", file.crc16);
        out.member("check_type", file.check_type);
        optional_text(out, "check_value", file.check_value);
        out.end_object();
    }
    out.end_array();
    out.key("unlisted_files").begin_array();
    for (const adl::set_maker::UnlistedFile& file : inspection.unlisted_files)
    {
        out.begin_object();
        out.member("name", file.name).member("pathname", file.pathname).member_int("member", file.member);
        out.member_int("size", static_cast<std::int64_t>(file.size));
        out.end_object();
    }
    out.end_array();
}

} // namespace

std::string inspection_document(const Verdict& verdict, std::string_view folder, const Inspection* inspection)
{
    JsonWriter out;
    out.begin_object();
    out.member_int("format", 1).member("kind", "adl-media-set-inspection");
    out.key("program").begin_object().member("name", "adl_make_set").member("version", version).end_object();
    out.member("result", verdict.completed ? "completed" : "failed").member("reason", verdict.reason);
    out.member("detail", verdict.detail).member_int("exit_code", verdict.exit_code).member("folder", folder);

    if (inspection != nullptr)
    {
        media_set(out, *inspection);
        loads(out, *inspection);
        batches(out, *inspection);
        files(out, *inspection);
        out.member("findings", inspection->findings);
    }
    else
    {
        out.member_null("media_set");
        out.key("loads").begin_array().end_array();
        out.key("batches").begin_array().end_array();
        out.key("files").begin_array().end_array();
        out.key("unlisted_files").begin_array().end_array();
        out.key("findings").begin_array().end_array();
    }
    out.end_object();
    return out.text();
}

} // namespace adl::make_set
