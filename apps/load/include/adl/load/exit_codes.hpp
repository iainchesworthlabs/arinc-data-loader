// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The program's exit codes (docs/load-command.md). 0 is success, 1 is "it ran and the outcome was a
// failure", 2 is wrong usage; the rest say what kept it from running as asked.

namespace adl::load
{

inline constexpr int exit_completed = 0;   // the operation completed (find: it ran)
inline constexpr int exit_failed = 1;      // the operation ran and ended in a failure
inline constexpr int exit_usage = 2;       // wrong options
inline constexpr int exit_media_set = 3;   // the media set could not be read, or changed after the check
inline constexpr int exit_no_answer = 4;   // the target did not answer
inline constexpr int exit_local_error = 5; // a port, a folder or the record file

} // namespace adl::load
