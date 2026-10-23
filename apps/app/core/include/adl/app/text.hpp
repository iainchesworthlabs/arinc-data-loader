#pragma once

// Conversion of the libraries' text to Qt strings. The libraries are free of Qt
// and hold their text as UTF-8 or ASCII.

#include <string_view>

#include <QString>

namespace adl::app {

// Text from the libraries (UTF-8 or ASCII, as views or strings) as a Qt string.
[[nodiscard]] inline QString to_qstring(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace adl::app
