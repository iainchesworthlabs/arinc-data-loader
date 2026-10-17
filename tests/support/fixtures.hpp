#pragma once

// The sample load, batch and media set the tests share. They are the scenario of
// tests/golden/make_golden.py, input for input: change one and the other has to
// change with it. They live in adl::samples (src/samples), where the program that
// writes sample payloads uses them too; the functions that build a load, which can
// fail in principle, are unwrapped here so that a test fails by throwing.

#include <string>
#include <utility>
#include <vector>

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/samples/scenarios.hpp"
#include "bytes.hpp"

namespace adl::test {

using namespace adl::arinc665;

using adl::samples::sample_batch;
using adl::samples::sample_batch_part_number;
using adl::samples::sample_data_1;
using adl::samples::sample_data_2;
using adl::samples::sample_load_part_number;
using adl::samples::sample_load_spec;
using adl::samples::sample_media_set_part_number;
using adl::samples::sample_support;

inline Load sample_load() {
    return adl::samples::sample_load().value();
}

inline MediaSetSpec sample_media_set_spec() {
    return adl::samples::sample_media_set_spec().value();
}

inline std::vector<std::byte> member_file(const MediaMember& member, const std::string& pathname,
                                          const std::string& name) {
    for (const MediaFile& file : member.files) {
        if (file.pathname == pathname && file.name == name) {
            return file.bytes;
        }
    }
    return {};
}

}  // namespace adl::test
