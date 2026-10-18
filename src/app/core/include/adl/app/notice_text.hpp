#pragma once

#include <cstdint>
#include <string_view>

#include <QString>

#include "adl/arinc615a/data_loader.hpp"

namespace adl::app {

// A notice of the engine as a line for the operator: what happened, to which file, and the number
// it carries said in the unit it is in (seconds, files, a transfer's outcome).
[[nodiscard]] QString notice_text(adl::arinc615a::Notice notice, std::string_view subject,
                                  std::uint32_t value);

}  // namespace adl::app
