#pragma once

// A file system that is the in-memory one until a test makes it misbehave: a path that cannot be
// looked at or made, and a hook that runs just before a directory is made (another process taking
// the name between the look and the make). It is what the error branches of the application's
// directory handling are tested through.

#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "adl/platform/files.hpp"
#include "fake_storage.hpp"

namespace adl::test {

class FaultyFileSystem final : public adl::platform::FileSystem {
public:
    explicit FaultyFileSystem(adl::fake::MemoryFileSystem& inner) : inner_(inner) {}

    // Paths (as given to the calls, matched at their end) and the error each call on them fails
    // with.
    std::map<std::string, adl::platform::ErrorCode> stat_fails;
    std::map<std::string, adl::platform::ErrorCode> make_directory_fails;
    std::map<std::string, adl::platform::ErrorCode> list_fails;
    // Run with the path before every make_directory call.
    std::function<void(std::string_view)> before_make_directory;
    // Run with the path after a make_directory call that succeeded.
    std::function<void(std::string_view)> after_make_directory;
    // Every make_directory call says the path exists, however it looks.
    bool make_directory_always_exists = false;

    adl::platform::Result<adl::platform::Owned<adl::platform::ReadFile>> open_read(
        std::string_view path) noexcept override {
        return inner_.open_read(path);
    }
    adl::platform::Result<adl::platform::Owned<adl::platform::WriteFile>> create(
        std::string_view path, adl::platform::CreateMode mode) noexcept override {
        return inner_.create(path, mode);
    }
    adl::platform::Result<adl::platform::FileInfo> stat(std::string_view path) noexcept override {
        if (const auto code = fault(stat_fails, path)) {
            return adl::platform::fail(*code);
        }
        return inner_.stat(path);
    }
    adl::platform::Result<void> make_directory(std::string_view path) noexcept override {
        if (before_make_directory) {
            before_make_directory(path);
        }
        if (make_directory_always_exists) {
            return adl::platform::fail(adl::platform::ErrorCode::already_exists);
        }
        if (const auto code = fault(make_directory_fails, path)) {
            return adl::platform::fail(*code);
        }
        auto made = inner_.make_directory(path);
        if (made && after_make_directory) {
            after_make_directory(path);
        }
        return made;
    }
    adl::platform::Result<void> remove(std::string_view path) noexcept override {
        return inner_.remove(path);
    }
    adl::platform::Result<void> rename(std::string_view from, std::string_view to) noexcept override {
        return inner_.rename(from, to);
    }
    adl::platform::Result<void> list(std::string_view directory,
                                     adl::platform::DirectoryVisitor visitor,
                                     void* context) noexcept override {
        if (const auto code = fault(list_fails, directory)) {
            return adl::platform::fail(*code);
        }
        return inner_.list(directory, visitor, context);
    }

private:
    static std::optional<adl::platform::ErrorCode> fault(
        const std::map<std::string, adl::platform::ErrorCode>& faults,
        std::string_view path) noexcept {
        for (const auto& [suffix, code] : faults) {
            if (path.ends_with(suffix)) {
                return code;
            }
        }
        return std::nullopt;
    }

    adl::fake::MemoryFileSystem& inner_;
};

}  // namespace adl::test
