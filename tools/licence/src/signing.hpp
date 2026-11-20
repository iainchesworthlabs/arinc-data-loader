// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The private key the tool signs with, and writing a signed file that is read back through the library
// before it is reported. The key is never printed, never written anywhere but its own file, and wiped from
// memory when it goes out of scope.

#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"
#include "options.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

class SigningKey
{
public:
    SigningKey() = default;
    SigningKey(const SigningKey&) = delete;
    SigningKey& operator=(const SigningKey&) = delete;
    ~SigningKey();

    // Loads the key the options name: the throwaway test key for --test, otherwise --key-id and --key-file.
    // The error is one sentence from messages.hpp.
    [[nodiscard]] std::expected<void, std::string> load(const Options& options);

    [[nodiscard]] const std::string& id() const noexcept
    {
        return id_;
    }
    [[nodiscard]] const adl::licence::signing::Seed& seed() const noexcept
    {
        return seed_;
    }
    [[nodiscard]] bool is_test() const noexcept
    {
        return test_;
    }

    // Sets the key from random bytes, for keygen.
    void set(std::string id, const adl::licence::signing::Seed& seed);

private:
    std::string id_;
    adl::licence::signing::Seed seed_{};
    bool test_ = false;
};

// What was written.
struct WrittenFile
{
    std::size_t bytes = 0;
};

// Whether `check` accepts the payload bytes of the file as read back; it returns why not, as a sentence.
using PayloadCheck = std::function<std::optional<std::string>(const std::string& payload)>;

// Signs `payload` for `purpose` with `key`, writes the file at `out` (which must not exist), reads it back
// through the library (envelope, signature against the key's public half, the same payload bytes), and then
// asks `check` about the payload. If anything fails the file is removed and the error says why.
[[nodiscard]] std::expected<WrittenFile, std::string> sign_and_write(std::string_view purpose, const SigningKey& key,
                                                                     const std::string& payload,
                                                                     const std::filesystem::path& out,
                                                                     const PayloadCheck& check);

} // namespace adl::licence_owner
