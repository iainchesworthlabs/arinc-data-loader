// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Making a media set from a plan, the way the generator's window always has: read every file, build the
// loads and the set, write the members to a folder beside the set's place, read that folder back with the
// loader's own check (adl::loader_core::check_media_set) and, only when the set verifies, move it into
// place. Whatever goes wrong, and whatever a Hooks object asks for, what had been written is removed, and
// a set only ever appears whole and verified. Nothing already there is overwritten.
//
// Failures are returned and never thrown. The words for them are the caller's: the window says them in
// its language and the command in English, from the kind and the facts a MakeFailure carries.

#include "adl/loader_core/media_set_check.hpp"
#include "adl/set_maker/plan.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace adl::set_maker
{

// Where the set goes.
struct Destination
{
    enum class Kind
    {
        exact,      // `path` is the folder that becomes the media set; it must be absent or empty
        first_free, // the set goes in the first free folder of `name`, `name_2`, ... below `path`
    };

    Kind kind = Kind::exact;
    std::filesystem::path path;
    std::filesystem::path name; // first_free only

    [[nodiscard]] static Destination exact(std::filesystem::path folder)
    {
        return Destination{Kind::exact, std::move(folder), {}};
    }
    [[nodiscard]] static Destination first_free(std::filesystem::path base, std::filesystem::path name)
    {
        return Destination{Kind::first_free, std::move(base), std::move(name)};
    }
};

// What a caller learns while a set is made, and how it stops it. The default of every function is to do
// nothing, so a caller overrides the ones it wants.
class Hooks
{
public:
    Hooks() = default;
    Hooks(const Hooks&) = delete;
    Hooks& operator=(const Hooks&) = delete;
    virtual ~Hooks() = default;

    // How much of the source files' bytes have been read, in whole percent (100 when there are none).
    virtual void progress(int percent)
    {
        static_cast<void>(percent);
    }

    // Called at the places where a job can be stopped: "read" (before each file), "allocate" (before the
    // buffer of a file is made: the answer is not looked at, and a hook may throw std::bad_alloc or
    // std::length_error to stand for a file that does not fit), "load", "write", "written" and "verified".
    // True stops the job at that place.
    [[nodiscard]] virtual bool checkpoint(std::string_view place)
    {
        static_cast<void>(place);
        return false;
    }

    // The members are on disk (beside the set's place, not yet checked).
    virtual void written(std::size_t members, std::size_t files, std::uint64_t bytes)
    {
        static_cast<void>(members);
        static_cast<void>(files);
        static_cast<void>(bytes);
    }

    // The loader's own check has read the written set and found it good.
    virtual void checked(const adl::loader_core::MediaSetReport& report)
    {
        static_cast<void>(report);
    }

    // An empty folder at the set's place is being taken over (exact only).
    virtual void took_over_empty_folder(const std::filesystem::path& folder)
    {
        static_cast<void>(folder);
    }
};

// What was removed after a failure that had written something: `attempted` is false when nothing had been
// written. `where` is the folder that was written, which is still there when `removed` is false.
struct Discard
{
    bool attempted = false;
    bool removed = false;
    std::filesystem::path where;
};

enum class FailureKind
{
    source_unreadable,     // `path`: a file could not be opened or read
    source_too_large,      // `path`: a file is too large to be read into memory (a length error)
    source_does_not_fit,   // `path`: the memory for a file could not be had
    no_data_files,         // a load has no data file
    load_not_built,        // `detail`: why adl::arinc665 refused a load
    set_not_built,         // `detail`: why adl::arinc665 refused the set
    output_exists,         // `path`: the set's place is there already and is not an empty folder
    not_written_below,     // `path`: the folder the set could not be written below; `at` and `system` say where and why
    too_many_unfinished,   // `path`: the name that has `.writing` to `.writing_100` beside it already
    write_failed,          // `at` and `detail`: the tree could not be written; `discard`
    read_back_failed,      // `at` and `detail`: the written tree could not be read; `discard`
    not_verified,          // `detail`: the written set does not verify; `discard`
    not_the_set_described, // `detail`: what the written set holds is not what the plan said; `discard`
    not_given_its_name,    // `path`, `at` and `system`: the folder could not be moved into place; `discard`
    cancelled,             // the hooks stopped it; `discard`
    out_of_memory,         // the computer ran out of memory
    error,                 // `detail`: an exception that no one expected
};

struct MakeFailure
{
    FailureKind kind = FailureKind::error;
    std::filesystem::path path; // the file or folder the failure is about, as the kind says
    std::filesystem::path at;   // the path that `system` or `detail` speaks of, when that is another
    std::string detail;         // the libraries' words, English
    std::string system;         // the operating system's words, in its own encoding; may be empty
    Discard discard;
};

struct MadeSet
{
    std::filesystem::path path; // where the set is
    std::size_t members = 0;
    std::size_t data_files = 0;
    std::size_t support_files = 0;
    std::size_t files = 0;                   // every file on every member
    std::uint64_t source_bytes = 0;          // what was read
    std::uint64_t bytes = 0;                 // what is on the members, the lists included
    adl::loader_core::MediaSetReport report; // the loader's own reading of it
};

// Makes the set. The plan is taken as valid (validate() says whether it is) but is not trusted with
// memory: a source that does not fit, or a failure of the libraries, comes back as a MakeFailure.
[[nodiscard]] std::expected<MadeSet, MakeFailure> make_set(const SetPlan& plan, const Destination& destination,
                                                           Hooks& hooks);

} // namespace adl::set_maker
