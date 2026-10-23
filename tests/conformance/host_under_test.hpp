#pragma once

// The conformance suite runs every check against two hosts: the real one this
// build selected (Windows, Linux or macOS: whichever platform CMake chose) and
// the fake. Both have to behave the same way, because the engine is tested on the
// fake and runs on the real one. A check that passes on the fake and fails on
// the real host is a fake that lies; one that passes on the real host and fails
// on the fake is the fake being too strict.
//
//     const auto kind = GENERATE(HostKind::fake, HostKind::real);
//     HostUnderTest under(kind);
//     auto& host = under.host();

#include "adl/platform/host.hpp"
#include "fake_host.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace adl::test
{

enum class HostKind
{
    fake,
    real
};

inline std::ostream& operator<<(std::ostream& out, HostKind kind)
{
    return out << (kind == HostKind::fake ? "fake host" : "real host");
}

class HostUnderTest
{
public:
    explicit HostUnderTest(HostKind kind) : kind_(kind)
    {
        if (kind == HostKind::fake)
        {
            fake_ = std::make_unique<fake::FakeSystem>();
            // The fake file system has no temporary directory; give it one.
            scratch_ = "/scratch";
            REQUIRE(fake_->host().files().make_directory(scratch_).has_value());
        }
        else
        {
            auto made = platform::make_host();
            REQUIRE(made.has_value());
            real_ = std::move(*made);
            temp_ = std::make_unique<TempDir>();
            scratch_ = temp_->path().generic_string();
        }
    }

    [[nodiscard]] HostKind kind() const noexcept
    {
        return kind_;
    }
    [[nodiscard]] bool is_fake() const noexcept
    {
        return kind_ == HostKind::fake;
    }
    [[nodiscard]] platform::Host& host() noexcept
    {
        return is_fake() ? static_cast<platform::Host&>(fake_->host()) : *real_;
    }
    [[nodiscard]] fake::FakeSystem& fake_system() noexcept
    {
        return *fake_;
    }

    // A directory that exists, is empty, and is the host's to write in.
    [[nodiscard]] const std::string& scratch() const noexcept
    {
        return scratch_;
    }
    [[nodiscard]] std::string in_scratch(std::string_view name) const
    {
        return scratch_ + "/" + std::string(name);
    }

private:
    HostKind kind_;
    std::unique_ptr<fake::FakeSystem> fake_;
    std::unique_ptr<platform::Host> real_;
    std::unique_ptr<TempDir> temp_;
    std::string scratch_;
};

} // namespace adl::test
