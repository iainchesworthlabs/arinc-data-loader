// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The factories the INTEGRITY-178B host (host_ghs178b.cpp) assembles its
// services from. Internal to libs/platform/ghs178b. UNBUILT AND UNTESTED:
// this port has never been compiled against the INTEGRITY-178B headers; the
// unverified assumptions are named in each implementation file and in
// docs/platform-ghs.md.

#include "adl/platform/files.hpp"
#include "adl/platform/memory.hpp"
#include "adl/platform/network.hpp"
#include "adl/platform/threads.hpp"
#include "adl/platform/time.hpp"

#include <memory>

namespace adl::platform::ghs178b
{

// make_network() takes no Clock: the sockets bound a receive time-out with
// GetClockTime on PrimaryClock, the same timer driver the host's Clock reads,
// so the two views agree.
[[nodiscard]] std::unique_ptr<Clock> make_clock() noexcept;
[[nodiscard]] std::unique_ptr<MemoryProvider> make_memory() noexcept;
[[nodiscard]] std::unique_ptr<FileSystem> make_files() noexcept;
[[nodiscard]] std::unique_ptr<Threading> make_threading(Clock& clock) noexcept;
[[nodiscard]] std::unique_ptr<Network> make_network() noexcept;

} // namespace adl::platform::ghs178b
