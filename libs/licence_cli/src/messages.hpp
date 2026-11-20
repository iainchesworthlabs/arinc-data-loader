// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Every sentence and every label adl_licence prints, in one place. English, and not translated: the windows have
// their own, translated, sentences for the same things. %1, %2 and so on are filled in by `format`. The guide
// page docs/user-guide/command-line.md has each of them, and tools/ci/check_messages.py holds the page to this
// file: a sentence added here fails the check until the page has it.

#include <initializer_list>
#include <string>
#include <string_view>

namespace adl::licence_cli
{

namespace message
{

// What --help prints.
inline constexpr std::string_view usage = R"(usage: adl_licence show
       adl_licence import <file>
       adl_licence remove <name>
       adl_licence verify <file>
       adl_licence --help
       adl_licence --version

Shows and manages the ARINC Data Loader licence on this computer. Every program of ARINC Data Loader reads the
same licence files. show and verify change nothing. A licence file is a record of what was bought: no program
needs one to work.

commands:
  show           the edition and the licence in force, and each licence file the programs find
  import <file>  check a licence file, or a list of withdrawn licences, and copy it to your licence folder
  remove <name>  remove a file from your licence folder, by the name show prints
  verify <file>  judge a file as a program would, and copy nothing

Exit codes: 0 done, 1 could not be done, 2 the command line is wrong.
ADL_SETTINGS_DIR moves the licence folders and the clock file, as it moves the settings of the programs.
)";

// Command line.
inline constexpr std::string_view no_command = "give a command";
inline constexpr std::string_view unknown_command = "unknown command %1";
inline constexpr std::string_view unknown_option = "unknown option %1";
inline constexpr std::string_view takes_no_argument = "%1 takes no argument";
inline constexpr std::string_view needs_one_file = "%1 needs the name of one file";
inline constexpr std::string_view needs_one_name = "remove needs the name of one licence file, as show prints it";
inline constexpr std::string_view help_alone = "%1 cannot be given with anything else";

// What the computer cannot do.
inline constexpr std::string_view cannot_start = "the files and the clock of this computer could not be opened";
inline constexpr std::string_view clock_unreadable =
    "the clock of this computer cannot be read, so a licence cannot be judged";
inline constexpr std::string_view no_folder = "this computer has no folder for licence files";

// A file named on the command line.
inline constexpr std::string_view cannot_read = "cannot read %1: %2";
inline constexpr std::string_view reason_missing = "it does not exist";
inline constexpr std::string_view reason_unreadable = "it is not a file, or it could not be read";
inline constexpr std::string_view reason_too_large = "it is larger than %1 bytes, which no licence file is";

// show.
inline constexpr std::string_view note = "Note:";
inline constexpr std::string_view note_no_keys =
    "This version of ARINC Data Loader cannot check licence files yet, so it accepts none. The Community edition "
    "works as it always did.";
inline constexpr std::string_view note_folder_full =
    "A licence folder holds more than %1 files; the rest were not read.";
inline constexpr std::string_view note_folder_unreadable = "A licence folder could not be read.";

// The lines of a licence's state: show and verify.
inline constexpr std::string_view label_edition = "Edition:";
inline constexpr std::string_view label_state = "State:";
inline constexpr std::string_view label_reason = "Reason:";
inline constexpr std::string_view label_licence = "Licence:";
inline constexpr std::string_view label_kind = "Kind:";
inline constexpr std::string_view label_licensed_to = "Licensed to:";
inline constexpr std::string_view label_term = "Term:";
inline constexpr std::string_view label_days_left = "Days left:";
inline constexpr std::string_view label_grace_ends = "Grace ends:";
inline constexpr std::string_view label_seats = "Seats:";
inline constexpr std::string_view label_features = "Features:";
inline constexpr std::string_view label_computers = "Computers:";
inline constexpr std::string_view label_updates_until = "Updates until:";
inline constexpr std::string_view label_files = "Files:";
inline constexpr std::string_view label_revocation_lists = "Revocation lists:";
inline constexpr std::string_view label_sequence = "Sequence:";
inline constexpr std::string_view label_licences_withdrawn = "Licences withdrawn:";
inline constexpr std::string_view label_keys_withdrawn = "Keys withdrawn:";
inline constexpr std::string_view seats_site = "site licence";
inline constexpr std::string_view none_word = "none";
inline constexpr std::string_view any_word = "any";
inline constexpr std::string_view file_user = "user";
inline constexpr std::string_view file_machine = "machine";
inline constexpr std::string_view file_in_use = "in use";
inline constexpr std::string_view lists_counted = "%1 accepted, %2 refused";
inline constexpr std::string_view term_range = "%1 to %2";

// Why a file is not taken (import and verify). The first two lines are the window's sentences.
inline constexpr std::string_view refused_newer =
    "This licence needs a newer version of ARINC Data Loader. Install the latest version, then import the licence "
    "again.";
inline constexpr std::string_view refused_revoked = "This licence was withdrawn. Contact support.";
inline constexpr std::string_view refused_wrong_computer =
    "This licence is for other computers. This computer is %1. Ask your administrator for a licence that names it.";
inline constexpr std::string_view refused_wrong_product =
    "This licence is for another product. Ask for an ARINC Data Loader licence.";
inline constexpr std::string_view refused_invalid =
    "This file is not a valid ARINC Data Loader licence. It may be damaged or incomplete. Ask for the file again.";
inline constexpr std::string_view folder_full =
    "The licence folder already holds %1 files. Remove one, then import again.";
inline constexpr std::string_view write_failed =
    "The licence could not be saved in %1. Check that the folder can be written.";

// import.
inline constexpr std::string_view imported_licence = "Imported licence %1 as %2.";
inline constexpr std::string_view imported_list =
    "Imported the list of withdrawn licences as %1. A licence on it is shown as withdrawn.";
inline constexpr std::string_view already_there =
    "That file is already in the licence folder as %1. Nothing was copied.";
inline constexpr std::string_view now_edition = "This is now the %1 edition.";
inline constexpr std::string_view edition_pro = "Pro";
inline constexpr std::string_view edition_enterprise = "Enterprise";
inline constexpr std::string_view ended_imported =
    "This licence ended on %1. It was imported, but it includes nothing now.";
inline constexpr std::string_view grace_imported =
    "This licence ended on %1. It was imported, and its grace period lasts until %2.";
inline constexpr std::string_view starts_on = "This licence starts on %1.";
inline constexpr std::string_view clock_looks_wrong =
    "The date on this computer looks wrong, so the licence cannot be judged yet. It was imported.";

// remove.
inline constexpr std::string_view removed =
    "Removed %1 from your licence folder. The file it was imported from was not touched.";
inline constexpr std::string_view remove_in_machine_folder =
    "%1 is in the licence folder of this computer, which only an administrator can change.";
inline constexpr std::string_view remove_not_found =
    "there is no file called %1 in your licence folder; show prints the names";
inline constexpr std::string_view remove_not_allowed =
    "%1 is not the name of a licence file: give a name as show prints it, with no folder";
inline constexpr std::string_view remove_failed = "the file %1 could not be removed";

// verify.
inline constexpr std::string_view would_accept_licence = "A program would accept this licence file.";
inline constexpr std::string_view would_accept_list = "A program would accept this list of withdrawn licences.";
inline constexpr std::string_view would_accept_already = "It is in the licence folder already as %1.";

// Unexpected.
inline constexpr std::string_view unexpected_failure = "unexpected failure: %1";

} // namespace message

// `text` with %1, %2 ... replaced by the values, in order.
[[nodiscard]] std::string format(std::string_view text, std::initializer_list<std::string_view> values);

} // namespace adl::licence_cli
