// SPDX-License-Identifier: GPL-3.0-or-later
// The operation made from what the operator chose: the strings the engine's request views are the plan's
// own, and choices the loader would refuse are refused before anything is sent.

#include "adl/loader_core/operation_plan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace adl::loader_core;
using adl::arinc615a::OperationKind;

namespace
{

std::vector<std::string> header_files_of(const adl::arinc615a::UploadRequest& request)
{
    std::vector<std::string> names;
    for (const adl::arinc615a::LoadReference& load : request.loads)
    {
        names.push_back(std::string{load.header_file_name} + " / " + std::string{load.load_part_number_name});
    }
    return names;
}

std::vector<std::string> file_names_of(const adl::arinc615a::DownloadRequest& request)
{
    std::vector<std::string> names;
    for (const adl::arinc615a::FileRequest& file : request.files)
    {
        names.emplace_back(file.file_name);
    }
    return names;
}

} // namespace

TEST_CASE("an upload names the loads in the order they were chosen", "[loader_core][operation_plan]")
{
    const std::vector<LoadChoice> choices{{"A.LUH", "PART-A"}, {"B.LUH", "PART-B"}, {"C.LUH", "PART-C"}};
    auto plan = OperationPlan::upload(choices);
    REQUIRE(plan.has_value());
    CHECK(plan->kind() == OperationKind::upload);
    const auto* upload = std::get_if<adl::arinc615a::UploadOperation>(&plan->operation());
    REQUIRE(upload != nullptr);
    const std::vector<std::string> expected{"A.LUH / PART-A", "B.LUH / PART-B", "C.LUH / PART-C"};
    CHECK(header_files_of(upload->loads) == expected);

    // The request views the plan's own copies: the choices can go, and so can the plan's place.
    OperationPlan moved = std::move(*plan);
    const auto* after = std::get_if<adl::arinc615a::UploadOperation>(&moved.operation());
    REQUIRE(after != nullptr);
    CHECK(header_files_of(after->loads) == expected);
}

TEST_CASE("an upload of no load is refused", "[loader_core][operation_plan]")
{
    const auto plan = OperationPlan::upload({});
    REQUIRE_FALSE(plan.has_value());
    CHECK(plan.error().kind == PlanError::Kind::no_loads);
    CHECK(plan.error().describe() == "No load was chosen.");
}

TEST_CASE("a download names the files in the order they were given", "[loader_core][operation_plan]")
{
    std::vector<std::string> names{"B.BIN", "A.BIN", "a-longer-file-name-than-a-string-keeps-in-itself.bin"};
    auto plan = OperationPlan::download_media(names);
    REQUIRE(plan.has_value());
    CHECK(plan->kind() == OperationKind::download_media);
    names.clear(); // the plan does not view the caller's strings
    OperationPlan moved = std::move(*plan);
    const auto* download = std::get_if<adl::arinc615a::MediaDownloadOperation>(&moved.operation());
    REQUIRE(download != nullptr);
    CHECK(file_names_of(download->files) ==
          std::vector<std::string>{"B.BIN", "A.BIN", "a-longer-file-name-than-a-string-keeps-in-itself.bin"});
}

TEST_CASE("a download of no file, or of a name that is not a plain file name, is refused",
          "[loader_core][operation_plan]")
{
    const auto none = OperationPlan::download_media({});
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().kind == PlanError::Kind::no_files);
    CHECK(none.error().describe() == "No file was named.");

    for (const std::string& bad : {std::string{"../A.BIN"}, std::string{"dir/A.BIN"}, std::string{""}})
    {
        const std::vector<std::string> names{"A.BIN", bad, "C.BIN"};
        const auto plan = OperationPlan::download_media(names);
        INFO("name: " << bad);
        REQUIRE_FALSE(plan.has_value());
        CHECK(plan.error().kind == PlanError::Kind::unsafe_file_name);
        CHECK(plan.error().name == bad); // the first one that is not safe
        CHECK(plan.error().describe() == "\"" + bad + "\" is not a safe file name.");
    }
}

TEST_CASE("an Information operation and an operator defined download need nothing chosen",
          "[loader_core][operation_plan]")
{
    const OperationPlan information = OperationPlan::information();
    CHECK(information.kind() == OperationKind::information);
    CHECK(std::holds_alternative<adl::arinc615a::InformationOperation>(information.operation()));

    const OperationPlan listed = OperationPlan::download_operator();
    CHECK(listed.kind() == OperationKind::download_operator);
    CHECK(std::holds_alternative<adl::arinc615a::OperatorDownloadOperation>(listed.operation()));
}
