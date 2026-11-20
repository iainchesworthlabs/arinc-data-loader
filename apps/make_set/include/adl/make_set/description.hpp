// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The description file of adl_make_set: a JSON file that says what media set to make (docs/file-reference.md,
// "The media set description"). It is read with the licence library's strict JSON reader, which refuses a
// key given twice, a comment, a trailing comma and the like, so no two readers can disagree about what the
// file says; this reader goes on to refuse a key it does not know, so that a mistyped key cannot make a set
// other than the one meant. What the file holds is a set_maker plan, checked by set_maker::validate() with the
// rules of the generator's window.
//
// A description holds no path of the computer it was written on: a file's `source` is relative, and is
// resolved against a root that the caller chooses.

#include "adl/set_maker/plan.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::make_set
{

// The limits of format 1 (docs/file-reference.md).
inline constexpr std::size_t max_description_bytes = 4U * 1024U * 1024U;
inline constexpr std::size_t max_loads = 100;
inline constexpr std::size_t max_files_in_a_load = 2000;
inline constexpr std::size_t max_files_in_all = 20000;
inline constexpr std::size_t max_target_hardware_ids = 64;
inline constexpr std::size_t max_batch_targets = 64;
inline constexpr std::size_t max_loads_in_a_batch_target = 100;
inline constexpr std::size_t max_source_bytes = 1024;
inline constexpr std::size_t max_text_bytes = 255; // part numbers, names, ids, comment, load type description

// What a description says: the plan, and the `source` of every file as it was written (the plan's own
// `source` has the root in front of it).
struct Description
{
    adl::set_maker::SetPlan plan;
    std::vector<std::vector<std::string>> sources; // for each load, for each file
};

// Why a description could not be used: one sentence for each thing found wrong, the first of them the
// first thing wrong in the file. At least one.
struct DescriptionError
{
    std::vector<std::string> sentences;
};

// Reads the text of a description. `source_root` is put in front of every `source`. Nothing is read from the
// disk.
[[nodiscard]] std::expected<Description, DescriptionError> read_description(std::string_view text,
                                                                            const std::filesystem::path& source_root);

// Reads the file. Without a root, the root is the folder that holds the file.
[[nodiscard]] std::expected<Description, DescriptionError>
read_description_file(const std::filesystem::path& file, const std::optional<std::filesystem::path>& source_root);

// A description as one line of JSON with every key written, so that reading it again gives the same plan.
[[nodiscard]] std::string write_description(const Description& description);

} // namespace adl::make_set
