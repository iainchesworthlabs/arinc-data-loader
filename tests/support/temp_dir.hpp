#pragma once

// A directory that exists for the length of a test and is removed after it.

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace adl::test
{

class TempDir
{
public:
    TempDir()
    {
        static std::atomic<unsigned> counter{0};
        std::random_device random;
        path_ = std::filesystem::temp_directory_path() /
                ("adl_test_" + std::to_string(random()) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace adl::test
