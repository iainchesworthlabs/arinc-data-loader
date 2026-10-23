#pragma once

// What an upload through LoaderModel needs: a media set (the sample load, with some data in it)
// written to a directory as a tree, the same files put into the fake host's file system under
// the same paths (the engine reads through the host, and the tests' host is in memory), and the
// plan that has the simulated target read the load.

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <QString>
#include <QUrl>

#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/platform/paths.hpp"
#include "adl/samples/scenarios.hpp"
#include "rig.hpp"
#include "temp_dir.hpp"

namespace adl::test {

struct UploadFixture {
    SampleLoad load = make_sample_load(5'000, 3);
    // Empty: the sample's own media set part number. Anything else rebuilds the set with that
    // part number, which is how a set with a finding is made (one the standard recommends
    // against, an I, Q or Z in the part number).
    std::string media_set_part_number;
    TempDir directory;

    [[nodiscard]] SimPlan plan() const {
        SimPlan plan;
        plan.loads = {load.item};
        return plan;
    }

    // Writes the tree, and mirrors every file into `rig`'s host. False if the tree could not be
    // written.
    [[nodiscard]] bool write(Rig& rig) const {
        std::vector<adl::arinc665::MediaMember> members = load.members;
        if (!media_set_part_number.empty()) {
            auto spec_value = adl::samples::sample_media_set_spec();
            auto spec = std::move(*spec_value);
            spec.media_set_part_number = adl::arinc665::PartNumber{media_set_part_number};
            spec.loads[0].load = load.load;
            auto made = adl::arinc665::build_media_set(spec);
            if (!made) {
                return false;
            }
            members = std::move(*made);
        }
        if (!adl::media_tree::write_media_set_tree(directory.path(), members)) {
            return false;
        }
        auto& files = rig.loader_host.fake_files();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory.path())) {
            if (!entry.is_regular_file()) {
                continue;
            }
            std::ifstream in(entry.path(), std::ios::binary);
            const std::vector<char> text{std::istreambuf_iterator<char>(in),
                                         std::istreambuf_iterator<char>()};
            std::vector<std::byte> bytes(text.size());
            std::memcpy(bytes.data(), text.data(), text.size());
            files.put(adl::platform::to_platform_path(entry.path()), bytes);
        }
        return true;
    }

    [[nodiscard]] QUrl url() const {
        return QUrl::fromLocalFile(QString::fromStdU16String(directory.path().u16string()));
    }
};

}  // namespace adl::test
