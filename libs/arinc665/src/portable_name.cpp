#include "adl/arinc665/portable_name.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace adl::arinc665 {
namespace {

[[nodiscard]] char lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

bool is_device_name(std::string_view name) noexcept {
    std::string stem{name.substr(0, name.find('.'))};
    std::ranges::transform(stem, stem.begin(), lower);
    constexpr std::array<std::string_view, 6> plain{"con", "prn",    "aux",
                                                    "nul", "conin$", "conout$"};
    if (std::ranges::find(plain, stem) != plain.end()) {
        return true;
    }
    return stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
           stem[3] >= '1' && stem[3] <= '9';
}

bool is_portable_file_name(std::string_view name) noexcept {
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') {
        return false;
    }
    constexpr std::string_view forbidden = "/\\:*?\"<>|";
    const bool characters_ok = std::ranges::all_of(name, [&](char c) {
        return c >= 0x20 && c <= 0x7E && forbidden.find(c) == std::string_view::npos;
    });
    return characters_ok && !is_device_name(name);
}

}  // namespace adl::arinc665
