// SPDX-License-Identifier: GPL-3.0-or-later
#include "job.hpp"

#include <algorithm>
#include <utility>

namespace adl::load
{

using adl::arinc615a::StatusCode;
using adl::loader_core::Severity;

Job::Job(EventWriter& events, adl::arinc615a::OperationKind operation, std::string target_address,
         std::string thw_id_pos, adl::loader_core::WallClock clock)
    : OperationRecorder(operation, std::move(target_address), std::move(thw_id_pos), std::move(clock)), events_(events)
{
}

void Job::flush_log()
{
    const auto& log = record().log;
    for (; logged_ < log.size(); ++logged_)
    {
        events_.log(log[logged_].severity, log[logged_].text);
    }
}

void Job::say(Severity severity, std::string text)
{
    add_log(severity, std::move(text));
    flush_log();
}

void Job::note_reason(StatusCode status, std::string_view description)
{
    if (adl::arinc615a::description_is_meaningful(status) && !description.empty())
    {
        target_text_ = std::string{description};
    }
}

void Job::choose_from_list(adl::loader_core::OperationGate& gate, bool all, std::vector<std::string> names)
{
    gate_ = &gate;
    pick_all_ = all;
    picks_ = std::move(names);
}

void Job::on_initialization(const adl::arinc615a::InitializationFile& file) noexcept
{
    OperationRecorder::on_initialization(file);
    note_reason(file.status, file.description);
    flush_log();
}

void Job::on_configuration_status(const adl::arinc615a::ConfigurationStatus& status) noexcept
{
    OperationRecorder::on_configuration_status(status);
    note_reason(status.status, status.description);
    flush_log();
}

void Job::on_upload_status(const adl::arinc615a::UploadStatus& status) noexcept
{
    OperationRecorder::on_upload_status(status);
    note_reason(status.status, status.description);
    flush_log();

    std::vector<adl::loader_core::RecordedLoad> loads;
    loads.reserve(status.loads.size());
    for (const adl::arinc615a::LoadProgress& load : status.loads)
    {
        loads.push_back(
            {std::string{load.load_part_number_name}, std::string{load.header_file_name}, load.status, load.ratio});
    }
    events_.progress(status.list_ratio, loads);
}

void Job::on_download_status(const adl::arinc615a::DownloadStatus& status) noexcept
{
    OperationRecorder::on_download_status(status);
    note_reason(status.status, status.description);
    flush_log();
}

void Job::on_configuration_list(const adl::arinc615a::ConfigurationList& list) noexcept
{
    OperationRecorder::on_configuration_list(list);
    flush_log();
    if (record().configuration)
    {
        events_.configuration(*record().configuration);
    }
}

adl::arinc615a::SelectionDecision Job::on_download_list(const adl::arinc615a::DownloadList& list,
                                                        adl::arinc615a::FileSelection& selection) noexcept
{
    const adl::arinc615a::SelectionDecision decision = OperationRecorder::on_download_list(list, selection);
    flush_log();

    std::vector<ListedFile> listed;
    for (const adl::arinc615a::FileListing& file : list.files)
    {
        listed.push_back({std::string{file.file_name}, std::string{file.description}});
    }
    events_.file_list(listed);

    if (gate_ == nullptr)
    {
        return decision;
    }
    // The choice: every file listed, or the files picked that are listed. A name that was picked and is not
    // listed is not asked for; it is told afterwards, in the reason the download ends with.
    std::vector<std::string> chosen;
    if (pick_all_)
    {
        for (const ListedFile& file : listed)
        {
            chosen.push_back(file.name);
        }
    }
    else
    {
        for (const std::string& name : picks_)
        {
            if (std::ranges::any_of(listed, [&name](const ListedFile& file) { return file.name == name; }))
            {
                chosen.push_back(name);
            }
            else
            {
                unlisted_.push_back(name);
            }
        }
    }
    chose_nothing_ = chosen.empty();
    // An empty choice is "none": the gate ends the operation with it.
    gate_->post_selection(std::move(chosen));
    return decision;
}

void Job::on_file_received(std::string_view name, const adl::arinc615a::ReceivedFile& file) noexcept
{
    OperationRecorder::on_file_received(name, file);
    flush_log();
    events_.file_received(record().files.back());
}

void Job::on_file_sent(std::string_view name, std::uint64_t bytes) noexcept
{
    OperationRecorder::on_file_sent(name, bytes);
    flush_log();
}

void Job::on_notice(adl::arinc615a::Notice notice, std::string_view subject, std::uint32_t value) noexcept
{
    OperationRecorder::on_notice(notice, subject, value);
    flush_log();
}

} // namespace adl::load
