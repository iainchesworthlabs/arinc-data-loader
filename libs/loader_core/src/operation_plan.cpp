// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/operation_plan.hpp"

#include "adl/arinc615a/directory_files.hpp"

namespace adl::loader_core
{

using adl::arinc615a::OperationKind;

std::string PlanError::describe() const
{
    switch (kind)
    {
    case Kind::no_loads:
        return "No load was chosen.";
    case Kind::no_files:
        return "No file was named.";
    case Kind::unsafe_file_name:
        return "\"" + name + "\" is not a safe file name.";
    }
    return {};
}

OperationPlan::OperationPlan(adl::arinc615a::Operation operation, OperationKind kind)
    : operation_(std::move(operation)), kind_(kind)
{
}

OperationPlan OperationPlan::information()
{
    return OperationPlan{adl::arinc615a::InformationOperation{}, OperationKind::information};
}

OperationPlan OperationPlan::download_operator()
{
    return OperationPlan{adl::arinc615a::OperatorDownloadOperation{}, OperationKind::download_operator};
}

std::expected<OperationPlan, PlanError> OperationPlan::upload(std::span<const LoadChoice> loads)
{
    if (loads.empty())
    {
        return std::unexpected(PlanError{PlanError::Kind::no_loads, {}});
    }
    OperationPlan plan{adl::arinc615a::InformationOperation{}, OperationKind::upload};
    // The names are all in place before anything views them.
    plan.strings_.reserve(loads.size() * 2);
    for (const LoadChoice& load : loads)
    {
        plan.strings_.push_back(load.header_file);
        plan.strings_.push_back(load.part_number);
    }
    plan.loads_.reserve(loads.size());
    for (std::size_t i = 0; i < loads.size(); ++i)
    {
        plan.loads_.push_back(adl::arinc615a::LoadReference{plan.strings_[2 * i], plan.strings_[2 * i + 1]});
    }
    plan.operation_ = adl::arinc615a::UploadOperation{adl::arinc615a::UploadRequest{plan.loads_}};
    return plan;
}

std::expected<OperationPlan, PlanError> OperationPlan::download_media(std::span<const std::string> names)
{
    if (names.empty())
    {
        return std::unexpected(PlanError{PlanError::Kind::no_files, {}});
    }
    // The names are checked before the operation starts: the request (and later the sink) would refuse a
    // name that is not a plain file name all the same.
    for (const std::string& name : names)
    {
        if (!adl::arinc615a::is_safe_file_name(name))
        {
            return std::unexpected(PlanError{PlanError::Kind::unsafe_file_name, name});
        }
    }
    OperationPlan plan{adl::arinc615a::InformationOperation{}, OperationKind::download_media};
    plan.strings_.assign(names.begin(), names.end());
    plan.files_.reserve(plan.strings_.size());
    for (const std::string& name : plan.strings_)
    {
        plan.files_.push_back(adl::arinc615a::FileRequest{name, {}});
    }
    plan.operation_ = adl::arinc615a::MediaDownloadOperation{adl::arinc615a::DownloadRequest{plan.files_}};
    return plan;
}

} // namespace adl::loader_core
