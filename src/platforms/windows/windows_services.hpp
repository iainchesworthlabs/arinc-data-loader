#pragma once

// What the Windows implementations share. Internal to src/platforms/windows: it
// names no Windows type (DWORD is an unsigned long), so including it does not
// pull <windows.h> into a translation unit that does not need it.

#include <memory>
#include <string_view>

#include "adl/platform/host.hpp"

namespace adl::platform::windows {

// GetLastError() values and WSAGetLastError() values, in terms of ErrorCode.
[[nodiscard]] ErrorCode map_win32(unsigned long error) noexcept;
[[nodiscard]] ErrorCode map_winsock(int error) noexcept;

[[nodiscard]] inline std::unexpected<Error> fail_win32(unsigned long error) noexcept {
    return fail(map_win32(error), static_cast<int>(error));
}
[[nodiscard]] inline std::unexpected<Error> fail_winsock(int error) noexcept {
    return fail(map_winsock(error), error);
}

// Milliseconds for a Windows wait: rounded up so that a wait never ends early,
// at most `limit`. A Duration of `forever` is not passed here.
[[nodiscard]] unsigned long to_milliseconds(Duration timeout, unsigned long limit) noexcept;

[[nodiscard]] std::unique_ptr<Clock> make_clock() noexcept;
[[nodiscard]] std::unique_ptr<MemoryProvider> make_memory() noexcept;
[[nodiscard]] std::unique_ptr<FileSystem> make_files() noexcept;
[[nodiscard]] std::unique_ptr<Threading> make_threading(Clock& clock) noexcept;

// Starts Winsock for as long as the returned object lives (WSAStartup is
// reference-counted, so any number of Hosts may be made); an error if it will
// not start.
[[nodiscard]] Result<std::unique_ptr<Network>> make_network(Clock& clock) noexcept;

}  // namespace adl::platform::windows
