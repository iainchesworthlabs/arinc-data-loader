#pragma once

// A directory that exists for the length of a test and is removed after it, for the RTOS
// test programs (tests/platform/zephyr, tests/platform/freertos).
//
// tests/support/temp_dir.hpp makes the directory with std::filesystem, which is the host
// operating system's file system. On an RTOS the platform's FileSystem is the file system
// under test, and the host's is not the one it sees, so this file replaces that one for
// those programs: they put tests/platform/rtos_support before tests/support on the include
// path, and `#include "temp_dir.hpp"` finds this. The directory is made, and removed with
// everything in it, through the platform's own FileSystem, under the mount point named by
// the ADL_TEST_SCRATCH_ROOT compile definition (a string, "/lfs").

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "adl/platform/files.hpp"
#include "adl/platform/host.hpp"

namespace adl::test {

// What HostUnderTest asks of the path: its text with '/' separators. (std::filesystem::path
// is more than a platform without a file system library needs.)
struct ScratchPath {
    std::string text;
    [[nodiscard]] std::string generic_string() const { return text; }
};

class TempDir {
public:
    TempDir() {
        static std::atomic<unsigned> counter{0};
        auto made = platform::make_host();
        if (made.has_value()) {
            host_ = std::move(*made);
            path_.text = std::string(root) + "/adl_test_" + std::to_string(counter++);
            (void)host_->files().make_directory(path_.text);
        }
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    ~TempDir() {
        if (host_) {
            remove_tree(path_.text);
        }
    }

    [[nodiscard]] const ScratchPath& path() const noexcept { return path_; }

private:
    static constexpr std::string_view root = ADL_TEST_SCRATCH_ROOT;

    // Removes a directory and everything under it. A test that failed halfway leaves
    // files and directories behind, and the next test must not meet them.
    void remove_tree(const std::string& directory) {
        platform::FileSystem& files = host_->files();
        std::vector<std::pair<std::string, platform::FileKind>> entries;
        (void)platform::for_each_entry(
            files, directory, [&](std::string_view name, platform::FileKind kind) {
                entries.emplace_back(directory + "/" + std::string(name), kind);
                return true;
            });
        for (const auto& [path, kind] : entries) {
            if (kind == platform::FileKind::directory) {
                remove_tree(path);
            } else {
                (void)files.remove(path);
            }
        }
        (void)files.remove(directory);
    }

    std::unique_ptr<platform::Host> host_;
    ScratchPath path_;
};

}  // namespace adl::test
