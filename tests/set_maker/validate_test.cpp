// SPDX-License-Identifier: GPL-3.0-or-later
// validate(): the rules of the generator's window, applied to a whole plan.

#include "adl/set_maker/validate.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

namespace
{

using namespace adl::set_maker;

// A plan that is good. The files are not looked at, so they need not exist.
SetPlan good_plan()
{
    SetPlan plan;
    plan.part_number = "ACM-SET-0001";
    plan.members = 2;
    PlannedLoad load;
    load.part_number = "ACM47-1234-5678";
    load.header_file_name = "ACM.LUH";
    load.target_hardware_ids = {"ACME"};
    load.files = {PlannedFile{"a", "APP.LUP", FileKind::data, {}},
                  PlannedFile{"b", "CONFIG.LDR", FileKind::support, {}}};
    plan.loads.push_back(load);
    plan.batch = PlannedBatch{"ACM4C-0100-0200", "ACM.LUB", "", derive_batch_targets(plan.loads)};
    return plan;
}

// The problem of `where`, or an empty text when there is none.
std::string problem_at(const SetPlan& plan, const std::string& where)
{
    for (const Problem& problem : validate(plan))
    {
        if (problem.where == where)
        {
            return problem.text;
        }
    }
    return {};
}

} // namespace

TEST_CASE("validate: a good plan has no problem", "[validate]")
{
    CHECK(validate(good_plan()).empty());
}

TEST_CASE("validate: the media set", "[validate]")
{
    SetPlan plan = good_plan();
    plan.part_number.clear();
    CHECK(problem_at(plan, "media_set.part_number") == "the media set part number is empty");
    plan.part_number = "ACM SET 0001"; // blanks
    CHECK_FALSE(problem_at(plan, "media_set.part_number").empty());
    plan.part_number = "ACM-SET-0001";
    plan.members = 0;
    CHECK_FALSE(problem_at(plan, "media_set.members").empty());
    plan.members = 256;
    CHECK_FALSE(problem_at(plan, "media_set.members").empty());
    plan.members = 255;
    CHECK(problem_at(plan, "media_set.members").empty());
    plan.loads.clear();
    plan.batch.reset();
    CHECK(problem_at(plan, "loads") == "the set has no load");
}

TEST_CASE("validate: a load", "[validate]")
{
    SetPlan plan = good_plan();
    plan.loads[0].part_number = "not a part number";
    CHECK_FALSE(problem_at(plan, "loads[0].part_number").empty());
    plan.loads[0].part_number = "ACM47-1234-5678";

    plan.loads[0].header_file_name = "CON.LUH"; // the standard allows it; no folder can hold it everywhere
    CHECK(problem_at(plan, "loads[0].header_file") == "this name will not work on every system");
    plan.loads[0].header_file_name = "ACM.TXT";
    CHECK_FALSE(problem_at(plan, "loads[0].header_file").empty());
    plan.loads[0].header_file_name.clear();
    CHECK(problem_at(plan, "loads[0].header_file") == "the header file name is empty");
    plan.loads[0].header_file_name = "ACM.LUH";

    plan.loads[0].target_hardware_ids = {"AC"};
    CHECK_FALSE(problem_at(plan, "loads[0].target_hardware_ids[0]").empty());
    plan.loads[0].target_hardware_ids = {"ACME", "ACME"};
    CHECK(problem_at(plan, "loads[0].target_hardware_ids[1]") == "this hardware id is given twice");
    plan.loads[0].target_hardware_ids = {};
    CHECK(problem_at(plan, "loads[0].target_hardware_ids[0]").empty());
}

TEST_CASE("validate: the files of a load", "[validate]")
{
    SetPlan plan = good_plan();
    plan.loads[0].files[0].name = "bad name.bin";
    CHECK_FALSE(problem_at(plan, "loads[0].files[0].name").empty());
    plan.loads[0].files[0].name = "DATA*.LUP"; // the standard lets it through, the folder tree does not
    CHECK(problem_at(plan, "loads[0].files[0].name") == "this name will not work on every system");
    plan.loads[0].files[0].name.clear();
    CHECK(problem_at(plan, "loads[0].files[0].name") == "the name is empty");
    plan.loads[0].files[0].name = "APP.LUP";
    // The same name in other capitals is the same file on Windows and macOS.
    plan.loads[0].files.push_back(PlannedFile{"c", "app.lup", FileKind::data, {}});
    CHECK(problem_at(plan, "loads[0].files[2].name") == "another file of this load has this name");
    plan.loads[0].files.pop_back();

    // The rule depends on the kind: AUX is a device name for any file, and LDR is for support files.
    plan.loads[0].files[1].name = "AUX.LDR";
    CHECK(problem_at(plan, "loads[0].files[1].name") == "this name will not work on every system");
    plan.loads[0].files[1].kind = FileKind::data;
    plan.loads[0].files[1].name = "CONFIG.LDR";
    CHECK_FALSE(problem_at(plan, "loads[0].files[1].name").empty()); // not a name for a data file

    plan.loads[0].files.clear();
    CHECK(problem_at(plan, "loads[0].files") == "the load has no data file");
    plan.loads[0].files = {PlannedFile{"b", "CONFIG.LDR", FileKind::support, {}}};
    CHECK(problem_at(plan, "loads[0].files") == "the load has no data file");
}

TEST_CASE("validate: several loads", "[validate]")
{
    SetPlan plan = good_plan();
    plan.loads.push_back(plan.loads[0]);
    CHECK(problem_at(plan, "loads[1].part_number") == "another load has this part number");
    CHECK(problem_at(plan, "loads[1].header_file") == "another load has this header file name");
    plan.loads[1].part_number = "ACM4C-0100-0200";
    plan.loads[1].header_file_name = "ACM2.LUH";
    // Two loads may have a file of one name each: they are in folders of their own.
    CHECK(validate(plan).empty());
}

TEST_CASE("validate: the batch", "[validate]")
{
    SetPlan plan = good_plan();
    plan.batch->part_number = "not a part number";
    CHECK_FALSE(problem_at(plan, "batch.part_number").empty());
    plan.batch->part_number = "ACM4C-0100-0200";
    plan.batch->file_name = "NUL.LUB";
    CHECK(problem_at(plan, "batch.file_name") == "this name will not work on every system");
    plan.batch->file_name = "ACM.LUH";
    CHECK_FALSE(problem_at(plan, "batch.file_name").empty());
    plan.batch->file_name = "ACM.LUB";

    plan.batch->targets.clear();
    CHECK(problem_at(plan, "batch.targets") == "the batch names no target");
    plan.batch->targets = {PlannedBatchTarget{"AC", {"ACM47-1234-5678"}}};
    CHECK_FALSE(problem_at(plan, "batch.targets[0].id").empty());
    plan.batch->targets = {PlannedBatchTarget{"ACME_L", {}}};
    CHECK(problem_at(plan, "batch.targets[0].loads") == "the target gets no load");
    plan.batch->targets = {PlannedBatchTarget{"ACME_L", {"ACM47-1234-5678", "ACM99-9999-9999"}}};
    CHECK(problem_at(plan, "batch.targets[0].loads[1]") == "no load of the set has this part number");
    CHECK(problem_at(plan, "batch.targets[0].loads[0]").empty());
}
