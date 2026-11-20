// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/set_maker/validate.hpp"

#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/naming.hpp"
#include "adl/arinc665/part_number.hpp"
#include "adl/arinc665/portable_name.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace adl::set_maker
{

namespace
{

constexpr unsigned most_members = 255;

std::string findings_text(const adl::arinc665::Findings& findings)
{
    std::string text;
    for (const adl::arinc665::Finding& finding : findings)
    {
        if (!text.empty())
        {
            text += "; ";
        }
        text += finding.clause + ": " + finding.message;
    }
    return text;
}

// Names that pass the standard's rules are printable ASCII (the portable rule asks for it), so a plain
// ASCII fold tells two names that differ in case only.
std::string folded(std::string_view name)
{
    std::string text{name};
    std::ranges::transform(text, text.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return text;
}

std::string at(std::string_view list, std::size_t index)
{
    return std::string{list} + "[" + std::to_string(index) + "]";
}

void add(std::vector<Problem>& problems, std::string where, std::string text)
{
    if (!text.empty())
    {
        problems.push_back(Problem{std::move(where), std::move(text)});
    }
}

constexpr std::string_view not_portable = "this name will not work on every system";

} // namespace

std::string check_name(NameKind kind, std::string_view name)
{
    adl::arinc665::Findings findings;
    switch (kind)
    {
    case NameKind::data_file:
        findings = adl::arinc665::check_data_file_name(name);
        break;
    case NameKind::support_file:
        findings = adl::arinc665::check_support_file_name(name);
        break;
    case NameKind::header_file:
        findings = adl::arinc665::check_header_file_name(name);
        break;
    case NameKind::batch_file:
        findings = adl::arinc665::check_batch_file_name(name);
        break;
    }
    if (!findings.empty())
    {
        return findings_text(findings);
    }
    // The standard's rules pass; the folder tree's rule is stricter.
    return adl::arinc665::is_portable_file_name(name) ? std::string{} : std::string{not_portable};
}

std::vector<Problem> validate(const SetPlan& plan)
{
    std::vector<Problem> problems;

    if (plan.part_number.empty())
    {
        add(problems, "media_set.part_number", "the media set part number is empty");
    }
    else
    {
        add(problems, "media_set.part_number",
            findings_text(adl::arinc665::check_media_set_part_number(plan.part_number)));
    }
    if (plan.members < 1 || plan.members > most_members)
    {
        add(problems, "media_set.members", "the number of members is not from 1 to 255");
    }
    if (plan.loads.empty())
    {
        add(problems, "loads", "the set has no load");
    }

    for (std::size_t i = 0; i < plan.loads.size(); ++i)
    {
        const PlannedLoad& load = plan.loads[i];
        const std::string here = at("loads", i);

        if (load.part_number.empty())
        {
            add(problems, here + ".part_number", "the load part number is empty");
        }
        else
        {
            add(problems, here + ".part_number",
                findings_text(adl::arinc665::check_load_part_number(load.part_number)));
            for (std::size_t other = 0; other < i; ++other)
            {
                if (plan.loads[other].part_number == load.part_number)
                {
                    add(problems, here + ".part_number", "another load has this part number");
                    break;
                }
            }
        }

        if (load.header_file_name.empty())
        {
            add(problems, here + ".header_file", "the header file name is empty");
        }
        else
        {
            add(problems, here + ".header_file", check_name(NameKind::header_file, load.header_file_name));
            for (std::size_t other = 0; other < i; ++other)
            {
                if (folded(plan.loads[other].header_file_name) == folded(load.header_file_name))
                {
                    add(problems, here + ".header_file", "another load has this header file name");
                    break;
                }
            }
        }

        for (std::size_t k = 0; k < load.target_hardware_ids.size(); ++k)
        {
            const std::string& id = load.target_hardware_ids[k];
            const std::string text = findings_text(adl::arinc665::check_target_hardware_id(id));
            add(problems, at(here + ".target_hardware_ids", k), text.empty() ? std::string{} : id + ": " + text);
            for (std::size_t other = 0; other < k; ++other)
            {
                if (load.target_hardware_ids[other] == id)
                {
                    add(problems, at(here + ".target_hardware_ids", k), "this hardware id is given twice");
                    break;
                }
            }
        }

        std::size_t data_files = 0;
        for (std::size_t k = 0; k < load.files.size(); ++k)
        {
            const PlannedFile& file = load.files[k];
            const std::string where = at(here + ".files", k) + ".name";
            data_files += file.kind == FileKind::data ? 1 : 0;
            if (file.name.empty())
            {
                add(problems, where, "the name is empty");
                continue;
            }
            const std::string text =
                check_name(file.kind == FileKind::data ? NameKind::data_file : NameKind::support_file, file.name);
            if (!text.empty())
            {
                add(problems, where, text);
                continue;
            }
            for (std::size_t other = 0; other < k; ++other)
            {
                if (folded(load.files[other].name) == folded(file.name))
                {
                    add(problems, where, "another file of this load has this name");
                    break;
                }
            }
        }
        if (data_files == 0)
        {
            add(problems, here + ".files", "the load has no data file");
        }
    }

    if (plan.batch)
    {
        const PlannedBatch& batch = *plan.batch;
        if (batch.part_number.empty())
        {
            add(problems, "batch.part_number", "the batch part number is empty");
        }
        else
        {
            add(problems, "batch.part_number", findings_text(adl::arinc665::check_load_part_number(batch.part_number)));
        }
        if (batch.file_name.empty())
        {
            add(problems, "batch.file_name", "the batch file name is empty");
        }
        else
        {
            add(problems, "batch.file_name", check_name(NameKind::batch_file, batch.file_name));
        }
        if (batch.targets.empty())
        {
            add(problems, "batch.targets", "the batch names no target");
        }
        for (std::size_t k = 0; k < batch.targets.size(); ++k)
        {
            const PlannedBatchTarget& target = batch.targets[k];
            const std::string here = at("batch.targets", k);
            const std::string text = findings_text(adl::arinc665::check_target_hardware_id(target.id));
            add(problems, here + ".id", text.empty() ? std::string{} : target.id + ": " + text);
            if (target.load_part_numbers.empty())
            {
                add(problems, here + ".loads", "the target gets no load");
            }
            for (std::size_t m = 0; m < target.load_part_numbers.size(); ++m)
            {
                const std::string& named = target.load_part_numbers[m];
                const bool known =
                    std::ranges::any_of(plan.loads, [&](const PlannedLoad& load) { return load.part_number == named; });
                if (!known)
                {
                    add(problems, at(here + ".loads", m), "no load of the set has this part number");
                }
            }
        }
    }
    return problems;
}

} // namespace adl::set_maker
