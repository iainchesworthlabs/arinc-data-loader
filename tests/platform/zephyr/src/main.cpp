// The Zephyr test program: Catch2 run on Zephyr's main thread, over the real platform layer
// (libs/platform/zephyr) on whatever board the program is built for. CI builds it for
// native_sim, where Zephyr's kernel, network stack and file system run as a host process.
//
// Before Catch2 starts, the file system the conformance suite writes to is made and mounted:
// littlefs on the board's storage partition at /lfs, freshly formatted so that a run does not
// meet the files of the last. Catch2's arguments come from the environment variable
// ADL_CATCH_ARGS (a native_sim process has an environment and Zephyr's main has no argv):
//
//     ADL_CATCH_ARGS="[conformance] --durations yes" ./zephyr.exe

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <catch2/catch_session.hpp>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(scratch_storage);

namespace {

fs_mount_t scratch_mount;

[[nodiscard]] bool mount_scratch_file_system() {
    const auto partition =
        static_cast<uintptr_t>(DT_FIXED_PARTITION_ID(DT_NODELABEL(storage_partition)));
    if (const int error = fs_mkfs(FS_LITTLEFS, partition, &scratch_storage, 0); error != 0) {
        std::printf("fs_mkfs failed: %d\n", error);
        return false;
    }
    scratch_mount.type = FS_LITTLEFS;
    scratch_mount.mnt_point = "/lfs";
    scratch_mount.fs_data = &scratch_storage;
    scratch_mount.storage_dev = reinterpret_cast<void*>(partition);
    if (const int error = fs_mount(&scratch_mount); error != 0) {
        std::printf("fs_mount failed: %d\n", error);
        return false;
    }
    return true;
}

// Splits the environment variable on blanks; empty when it is not set.
[[nodiscard]] std::vector<std::string> catch_arguments() {
    std::vector<std::string> arguments{"adl_zephyr_tests"};
    const char* text = std::getenv("ADL_CATCH_ARGS");
    if (text == nullptr) {
        return arguments;
    }
    std::string current;
    for (const char* c = text;; ++c) {
        if (*c == ' ' || *c == '\0') {
            if (!current.empty()) {
                arguments.push_back(std::move(current));
                current.clear();
            }
            if (*c == '\0') {
                break;
            }
        } else {
            current.push_back(*c);
        }
    }
    return arguments;
}

}  // namespace

int main() {
    int status = 2;
    if (mount_scratch_file_system()) {
        const std::vector<std::string> arguments = catch_arguments();
        std::vector<const char*> argv;
        for (const std::string& argument : arguments) {
            argv.push_back(argument.c_str());
        }
        Catch::Session session;
        status = session.run(static_cast<int>(argv.size()), argv.data());
    }
    std::fflush(stdout);
    // A native_sim process ends when it is told to, with the code the harness reads; on a
    // board there is nowhere to exit to, and this is the end of the program.
    std::exit(status);
}
