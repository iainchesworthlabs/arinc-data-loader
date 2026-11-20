// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// Every sentence the owner tool says, in one place. %1, %2 and so on are filled in by `format`. The
// guide page docs/licence-owner-tool.md lists each of them, and tools/ci/check_messages.py holds the page to
// this file: a sentence added here fails the check until the page has it.

#include <initializer_list>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

namespace message
{

// The first line of every output of a command that was given --test.
inline constexpr std::string_view test_note =
    "NOTE: --test uses the throwaway key test-rfc8032-1 of RFC 8032. No ARINC Data Loader program accepts what it "
    "signs.";

// What --help prints.
inline constexpr std::string_view usage = R"(usage: adl_licence_owner <command> [options]

The owner's tool for ARINC Data Loader licence files. It is built with the tools and is never installed or shipped.

commands:
  inspect <file>   show what a licence file or a revocation list says
  verify <file>    judge a licence file as a program would, or check a revocation list
  issue            sign a licence request into a licence file
  revocations      sign a revocation list request into a revocation list file
  keygen           make a key pair for signing

options:
  inspect <file> [--test] [--trust <file>]...
  verify <file> [--test] [--trust <file>]... [--now <time>] [--computer <name>]
                [--latest-seen <time>] [--build-time <time>] [--revocations <file>]
  issue --request <file> --out <file> (--test | --key-id <id> --key-file <file>) [--dry-run]
  revocations --request <file> --out <file> (--test | --key-id <id> --key-file <file>) [--dry-run]
  keygen --key-id <id> --purpose licence|revocations --out <folder>

--test signs and checks with the throwaway key test-rfc8032-1, which no program accepts.
Times are written like 2026-11-13T02:10:00Z. File names and output are UTF-8.
Exit codes: 0 done, 1 could not be done, 2 the command line is wrong.
)";

// Command line.
inline constexpr std::string_view help_hint = "Run adl_licence_owner --help for the commands.";
inline constexpr std::string_view no_command = "give a command";
inline constexpr std::string_view unknown_command = "unknown command %1";
inline constexpr std::string_view unknown_option = "unknown option %1 for %2";
inline constexpr std::string_view option_needs_value = "%1 needs a value";
inline constexpr std::string_view option_twice = "%1 was given twice";
inline constexpr std::string_view needs_option = "%1 needs %2";
inline constexpr std::string_view needs_one_file = "%1 needs one file";
inline constexpr std::string_view needs_no_file = "%1 takes no file name";
inline constexpr std::string_view test_or_key = "--test cannot be used with --key-id or --key-file";
inline constexpr std::string_view needs_key_pair = "%1 needs --test, or --key-id and --key-file";
inline constexpr std::string_view bad_time = "%1 is not a time like 2026-11-13T02:10:00Z";
inline constexpr std::string_view bad_key_id =
    "%1 is not a key id: use lower-case letters, digits and hyphens, 64 characters at most";
inline constexpr std::string_view bad_purpose = "%1 is not a purpose: use licence or revocations";
inline constexpr std::string_view bad_argument = "an argument is not text that can be used as a file name";

// Files.
inline constexpr std::string_view cannot_read = "cannot read %1: %2";
inline constexpr std::string_view cannot_write = "cannot write %1: %2";
inline constexpr std::string_view reason_missing = "it does not exist";
inline constexpr std::string_view reason_not_a_file = "it is not a file";
inline constexpr std::string_view reason_unreadable = "it could not be read";
inline constexpr std::string_view reason_too_large = "it is larger than %1 bytes";
inline constexpr std::string_view reason_exists = "it exists already, and the tool never writes over a file";
inline constexpr std::string_view reason_unwritable = "it could not be written";

// inspect and verify.
inline constexpr std::string_view signature_verifies = "verifies";
inline constexpr std::string_view signature_not_checked = "not checked: no key with this id and purpose is known";
inline constexpr std::string_view signature_fails = "does not verify: %1";
inline constexpr std::string_view payload_unreadable = "not readable: %1";
inline constexpr std::string_view purpose_unknown = "this tool does not know what a file for the purpose %1 holds";
inline constexpr std::string_view revocations_refused = "the revocation list %1 is refused: %2";
inline constexpr std::string_view trust_refused = "the public key file %1 is refused: %2";
inline constexpr std::string_view trust_bad_form =
    "it is not a public key file as keygen writes it (one line with key_id, purpose and public_key)";

// issue and revocations.
inline constexpr std::string_view request_refused = "the request %1 is refused: %2";
inline constexpr std::string_view request_not_read = "the library does not read it as a request (%1)";
inline constexpr std::string_view product_wrong = "the product is %1, and a licence from this tool is for adl";
inline constexpr std::string_view id_wrong = "the licence number %1 is not of the form ADL-2026-000123";
inline constexpr std::string_view id_in_test_range =
    "the licence number %1 is in the range 900000 to 999999, which is kept for test licences";
inline constexpr std::string_view key_not_for_test = "the key id %1 begins with test-, which is only for --test";
inline constexpr std::string_view key_file_refused = "the key file %1 is refused: %2";
inline constexpr std::string_view key_file_not_a_key = "it does not hold a key as keygen writes it";
inline constexpr std::string_view sequence_zero =
    "the sequence number is 0, which is the empty list every release carries: use 1 or more";
inline constexpr std::string_view dry_run_done = "Dry run: nothing was signed and nothing was written.";
inline constexpr std::string_view read_back_failed =
    "the file written to %1 was removed, because reading it back failed: %2";
inline constexpr std::string_view read_back_differs = "it does not say what the request says";
inline constexpr std::string_view cannot_sign = "the request could not be written as a signed file: %1";
inline constexpr std::string_view wrote_file = "Wrote: %1";

// keygen.
inline constexpr std::string_view folder_missing =
    "the folder %1 does not exist: make it first, on the drive that will keep the key";
inline constexpr std::string_view folder_in_repository =
    "the folder %1 is inside a git repository: a key made there could be committed by mistake. Choose a folder "
    "that no repository holds";
inline constexpr std::string_view no_random = "this computer gives no random numbers: no key was made";
inline constexpr std::string_view random_refused = "the random numbers were not usable: no key was made";
inline constexpr std::string_view key_written_private = "Private key: %1";
inline constexpr std::string_view key_private_advice1 =
    "  It is not shown here and is never logged. Keep it offline, with two encrypted copies on separate media.";
inline constexpr std::string_view key_private_advice2 =
    "  Never put it in a repository, an email or a chat. Whoever holds it can make licences that programs accept.";
inline constexpr std::string_view key_written_public = "Public key file: %1";
inline constexpr std::string_view key_public_advice = "  It is not secret. Keep it with the licence records.";
inline constexpr std::string_view key_trust_advice1 =
    "To make programs accept licences signed with this key, add its public half to builtin_keys() in";
inline constexpr std::string_view key_trust_advice2 =
    "libs/licence/src/trusted_keys.cpp, with this key id, this purpose and the public key above, and build a release.";
inline constexpr std::string_view key_standby_advice =
    "Make a standby key now with the same command and another id, and add its public half to the same list, so "
    "that a leak of this key can be answered at once.";
inline constexpr std::string_view key_removed = "the key files were removed, because %1";
inline constexpr std::string_view key_check_failed = "the key file written could not be read back";

// Unexpected.
inline constexpr std::string_view unexpected_failure = "unexpected failure: %1";

} // namespace message

// `text` with %1, %2 ... replaced by the values, in order.
[[nodiscard]] std::string format(std::string_view text, std::initializer_list<std::string_view> values);

} // namespace adl::licence_owner
