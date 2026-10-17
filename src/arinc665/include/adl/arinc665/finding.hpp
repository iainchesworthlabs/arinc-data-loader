#pragma once

#include <string>
#include <vector>

namespace adl::arinc665 {

// Something in a file, a load or a media set that the standard says it "should"
// be otherwise. A finding is not an error: such a file can still be read and
// written, and loaders are expected to cope with it (section 2.1.1 says an
// ARINC 615A loader should not refuse a part number for its format). Where the
// standard says a file "must" be some way, or the layout cannot be read without
// it, the operation fails with an Error instead.
struct Finding {
    std::string clause;   // the section of ARINC 665-3, for example "2.1.1"
    std::string message;  // what differs, in a sentence

    friend bool operator==(const Finding&, const Finding&) = default;
};

using Findings = std::vector<Finding>;

}  // namespace adl::arinc665
