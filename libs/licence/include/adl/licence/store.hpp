// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The licence store: the licence files in a user's folder and in a folder an administrator keeps, the best
// one of them, importing a file, removing one, and the clock file the clock check keeps between runs.
//
// The store reaches files through the small interface below and no other way. The caller fills it in, over
// whatever file system it has, so this library includes no file-system header and every case, a full disk
// included, can be tested. The folders are given as text, and so is each file name: a path is a folder, '/'
// and a name.
//
// All of that text is UTF-8, and this library passes it on as it is: it never converts a name, through a code
// page or any other way, so a folder or a file with letters outside ASCII is what the caller's `Files` makes of
// those bytes. A caller on Windows has to take the bytes as UTF-8 and not as the code page's.
//
// Two file types are the store's: a licence file and a signed revocation list, by their extensions
// (product.hpp). A file with any other extension in a licence folder is not read.
//
// Nothing here deletes a licence file on its own. An import never replaces a file, and a removal works on
// one file in the user's folder, by name.

#include "adl/licence/error.hpp"
#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace adl::licence::store
{

// At most this many files with a licence or revocation extension are read from a folder, in name order. A
// folder that holds more is reported, and an import into a full folder is refused, so that a file the
// store wrote is never one it then ignores.
inline constexpr std::size_t max_files_per_folder = 64;

enum class FilesError
{
    not_found,      // there is no such file or folder
    already_exists, // `create` found the name taken
    too_large,      // `read` found more bytes than it was allowed
    failed,         // anything else: no space, no permission, a failing disk
};

template <typename T>
using FilesResult = std::expected<T, FilesError>;

// What the store needs of a file system. A path is a folder name, '/' and a file name, in UTF-8, and the
// caller's platform understands it as it is given. The calls need not be safe to make from two threads at once.
class Files
{
public:
    virtual ~Files() = default;

    // The names of the regular files in `folder`, in no particular order. `not_found` for a folder that is
    // not there.
    [[nodiscard]] virtual FilesResult<std::vector<std::string>> list(std::string_view folder) = 0;

    // The whole file. `too_large` when it holds more than `max_bytes`, and then nothing is read past that.
    [[nodiscard]] virtual FilesResult<std::string> read(std::string_view path, std::size_t max_bytes) = 0;

    // Writes a new file, and its folder and the ones above it if they are not there. Never replaces:
    // `already_exists` when the name is taken. A file that could not be written whole is not left behind.
    [[nodiscard]] virtual FilesResult<void> create(std::string_view path, std::string_view content) = 0;

    // Writes the file, replacing one that is there, so that a reader sees the old file or the new one and
    // never half of it. Makes the folder if it is not there.
    [[nodiscard]] virtual FilesResult<void> replace(std::string_view path, std::string_view content) = 0;

    // Removes one file. `not_found` when there is none.
    [[nodiscard]] virtual FilesResult<void> remove(std::string_view path) = 0;
};

// Where the licences are. The text is what `Files` takes: '/' between a folder and a name.
struct Folders
{
    // Imports go here, and a removal works here. May not exist yet.
    std::string user;
    // The folder an administrator keeps for every user of the computer. Read, never written. Empty for none.
    std::string machine;
    // The clock file (a path, not a folder). Empty for none: then the clock check has only what a licence and
    // the build carry.
    std::string clock_file;
};

enum class Origin
{
    user,
    machine,
};

// One licence file found, and what it amounts to.
struct Entry
{
    Origin origin = Origin::user;
    // The file's name in its folder.
    std::string name;
    // The state the file is in. A file that could not be read or is not a licence is `invalid`, with the
    // reason in the status; one that is too large is `invalid` too.
    Status status;
};

// What a scan found.
struct Overview
{
    // The licence files, the user's folder first, each folder in name order.
    std::vector<Entry> licences;
    // The best of them, as `best` chooses; no_licence() when there is no licence file.
    Status best;
    // The list in force: the one the caller passed (or the one built into this release), or a signed list
    // found in a folder if its sequence number is higher.
    RevocationList revocations;
    // How many signed revocation lists were read and accepted, and how many were refused.
    int revocation_lists_accepted = 0;
    int revocation_lists_refused = 0;
    // A folder held more than `max_files_per_folder` files with the extensions the store reads, and the rest
    // were not read.
    bool folder_full = false;
    // A folder could not be listed for a reason other than being absent. An absent folder is no fault.
    bool folder_unreadable = false;
};

// Reads the licence files and revocation lists in both folders and judges each licence in the context
// `where`. `where.keys` holds the trusted keys for both purposes. The signed revocation lists found take part
// in the judgement: a licence withdrawn by a list in a folder is `invalid`. `where.revocations` is the list to
// start from; null starts from `builtin_revocations()`. `where.revocations` is not used after the call.
[[nodiscard]] Overview scan(Files& files, const Folders& folders, Context where);

enum class ImportFailure
{
    refused,      // the file is not one the store takes: `reason` says why
    folder_full,  // the user's folder already holds `max_files_per_folder` files
    write_failed, // the file was fine and could not be written
};

struct ImportError
{
    ImportFailure failure = ImportFailure::refused;
    // For `refused`, why: the reason of the status the file amounts to (malformed, newer_version,
    // bad_signature, revoked, wrong_computer, wrong_product). Reason::ok otherwise.
    Reason reason = Reason::ok;
};

struct Imported
{
    // The name the file has in the user's folder.
    std::string name;
    // A signed revocation list, not a licence.
    bool revocation_list = false;
    // The user's folder already held a file with these bytes, so nothing was written.
    bool already_there = false;
    // What the file amounts to now. For a revocation list, `none`. `value` is set for a licence: the caller
    // can give it to `apply_reset` to apply a clock reset, and save the result with `save_clock`.
    Status status;
};

// Puts a copy of a signed licence file or revocation list into the user's folder.
//
// The file has to be the signed file of this product that a program could use: the envelope and signature
// have to verify and, for a licence, the payload has to read and name this product. A licence that has ended,
// has not started, or is judged while the clock looks wrong is taken: it is the customer's file, and a screen
// says what state it is in. One that is withdrawn, names another computer, or needs a newer version is
// refused with the reason.
//
// A licence is saved as `<licence number><licence extension>`, a list as `revocations-<sequence><extension>`.
// If that name is taken by a file with other bytes, the next free name `<name>-2`, `-3`, ... is used: an
// import never replaces a file and never deletes one. A file with the same bytes is not written again.
[[nodiscard]] std::expected<Imported, ImportError> import_file(Files& files, const Folders& folders,
                                                               std::string_view file_text, const Context& where);

enum class RemoveError
{
    not_allowed, // not the name of a licence or revocation file: a path, a dot name, another extension
    not_found,   // no such file in the user's folder
    failed,      // the file could not be removed
};

// Removes one file from the user's folder, by the name `scan` gave. Nothing in the machine folder is touched,
// whatever the name, and nothing is removed that is not named with a licence or revocation extension.
[[nodiscard]] std::expected<void, RemoveError> remove_file(Files& files, const Folders& folders, std::string_view name);

// The clock file: `{"format": 1, "latest_seen": "<time>", "applied_resets": ["<token>", ...]}`. The reader
// refuses what the strict JSON reader refuses, a format it does not know, and more than `max_applied_resets`
// tokens. `latest_seen` is absent when nothing was seen.
[[nodiscard]] Result<ClockState> parse_clock(std::string_view text);
[[nodiscard]] std::string write_clock(const ClockState& state);

// The stored clock state. A missing, unreadable or damaged file is the default state (nothing seen): the
// design accepts that the file is plain data and can be deleted.
[[nodiscard]] ClockState load_clock(Files& files, const Folders& folders);

// Writes the clock file. `failed` when `folders.clock_file` is empty or the file could not be written.
[[nodiscard]] FilesResult<void> save_clock(Files& files, const Folders& folders, const ClockState& state);

} // namespace adl::licence::store
