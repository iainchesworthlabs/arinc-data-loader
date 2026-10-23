#pragma once

// The names a file may have on every platform this project runs on, so that a tree written on one
// can be read, copied and written on any. The ARINC standards allow more than a Windows host
// does (a name can hold a colon or be CON), and a name that arrives from a target is not the
// operator's to trust: a colon names an NTFS alternate data stream, a device name opens a
// device, a trailing dot or space is dropped by Windows and so names another file.

#include <string_view>

namespace adl::arinc665 {

// CON, PRN, AUX, NUL, COM1 to COM9, LPT1 to LPT9, CONIN$ and CONOUT$ in any case, with or
// without an extension: on Windows these are devices, whatever directory they are named in.
[[nodiscard]] bool is_device_name(std::string_view name) noexcept;

// Whether `name` is one file name that is the same file on every platform: printable ASCII,
// none of the characters / \ : * ? " < > |, not "." or "..", not ending in a dot or a space, and
// not a device name. A name is a name and nothing more; it is not a path. The case of a name is
// not significant to the answer (two names that differ only in case are one file on Windows and
// macOS), and the length is not limited here.
[[nodiscard]] bool is_portable_file_name(std::string_view name) noexcept;

}  // namespace adl::arinc665
