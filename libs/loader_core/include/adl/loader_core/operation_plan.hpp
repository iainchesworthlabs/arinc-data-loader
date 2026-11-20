// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The operation the loader is asked to run, made from what the operator chose: nothing, for an
// Information operation; the loads to upload; the files to download, named or left to the target's list.
// The engine's requests only view their strings (a load's header file name and part number, a file's
// name), so a plan keeps its own copies, and the Operation it hands out is good for as long as the plan
// is. Choices that the loader would refuse anyway are refused here, before anything is sent: no load, no
// file, a file name that is not a plain name (adl::arinc615a::is_safe_file_name).

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/protocol_files.hpp"

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace adl::loader_core
{

// A load to upload, as the operator names it: its header file and its part number.
struct LoadChoice
{
    std::string header_file;
    std::string part_number;
};

// Why a choice made no operation.
struct PlanError
{
    enum class Kind
    {
        no_loads,         // an upload of nothing
        no_files,         // a download of no file
        unsafe_file_name, // `name` is not a plain file name
    };

    Kind kind = Kind::no_loads;
    std::string name; // unsafe_file_name: the name as it was given

    // "No load was chosen.", "No file was named.", "\"../A\" is not a safe file name.": the sentences the
    // window says, in English.
    [[nodiscard]] std::string describe() const;
};

class OperationPlan
{
public:
    // An Information operation (6.3.1): asks for the target's configuration.
    [[nodiscard]] static OperationPlan information();

    // An Upload (6.3.2) of `loads`, in this order. Fails with no_loads if there are none.
    [[nodiscard]] static std::expected<OperationPlan, PlanError> upload(std::span<const LoadChoice> loads);

    // A media defined Download (6.3.3) of the files `names` name, in this order. Fails with no_files if
    // there are none, and with unsafe_file_name for the first name that is not a plain file name.
    [[nodiscard]] static std::expected<OperationPlan, PlanError> download_media(std::span<const std::string> names);

    // An operator defined Download (6.3.4): the target lists its files and the operator chooses.
    [[nodiscard]] static OperationPlan download_operator();

    // A plan can be moved, not copied: what the operation views is what the plan owns, and a move keeps
    // it where it is.
    OperationPlan(const OperationPlan&) = delete;
    OperationPlan& operator=(const OperationPlan&) = delete;
    OperationPlan(OperationPlan&&) noexcept = default;
    OperationPlan& operator=(OperationPlan&&) noexcept = default;
    ~OperationPlan() = default;

    // What to hand to the runner. Valid for as long as the plan is, and after it is moved.
    [[nodiscard]] const adl::arinc615a::Operation& operation() const noexcept
    {
        return operation_;
    }
    [[nodiscard]] adl::arinc615a::OperationKind kind() const noexcept
    {
        return kind_;
    }

private:
    OperationPlan(adl::arinc615a::Operation operation, adl::arinc615a::OperationKind kind);

    adl::arinc615a::Operation operation_;
    adl::arinc615a::OperationKind kind_;
    // What `operation_` views. Vectors keep their elements where they are when they are moved.
    std::vector<std::string> strings_;
    std::vector<adl::arinc615a::LoadReference> loads_;
    std::vector<adl::arinc615a::FileRequest> files_;
};

} // namespace adl::loader_core
