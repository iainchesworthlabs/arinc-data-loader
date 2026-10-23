// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the harnesses under libs/<lib>/fuzz share: the fuzzer's bytes, as the span of bytes a decoder
// takes. A decoder fails by returning an error, never by crashing; the sanitizers the harness is
// built with are what notice the day it does.

#include <cstddef>
#include <cstdint>
#include <span>

namespace adl::test
{

[[nodiscard]] inline std::span<const std::byte> fuzz_bytes(const std::uint8_t* data, std::size_t size) noexcept
{
    return size == 0 ? std::span<const std::byte>{} : std::as_bytes(std::span<const std::uint8_t>{data, size});
}

// Decodes with `decode` and does nothing else: for a decoder whose result has no checker.
template <auto Decode>
int fuzz_decode_only(const std::uint8_t* data, std::size_t size)
{
    static_cast<void>(Decode(fuzz_bytes(data, size)));
    return 0;
}

} // namespace adl::test
