#pragma once

// The Host is everything the loader needs from the system it runs on, gathered
// in one object so that the engine is handed a single reference and a test is
// handed a different one. A real Host is made by make_host(), which each
// platform defines; a fake one (tests/fake) drives the whole engine without a
// socket or a real second.
//
// The services are separate interfaces rather than one, because most code
// needs one of them: the codec needs none, the protocol core needs none (it is
// driven by time and bytes handed to it), the runner needs the clock and the
// network, the file source needs files.

#include <memory>

#include "adl/platform/error.hpp"
#include "adl/platform/files.hpp"
#include "adl/platform/ipc.hpp"
#include "adl/platform/memory.hpp"
#include "adl/platform/network.hpp"
#include "adl/platform/threads.hpp"
#include "adl/platform/time.hpp"

namespace adl::platform {

// What a Host is, for a log line and for the few decisions that depend on it.
struct HostInfo {
    const char* name = "";  // "windows", "linux", "macos" or "fake"
    // The platform has real-time scheduling worth the name. A desktop has not:
    // it can be late, and the loader's time-outs are written to tolerate that.
    bool real_time = false;
};

class Host {
public:
    virtual ~Host() = default;

    [[nodiscard]] virtual HostInfo info() const noexcept = 0;

    [[nodiscard]] virtual Clock& clock() noexcept = 0;
    [[nodiscard]] virtual MemoryProvider& memory() noexcept = 0;
    [[nodiscard]] virtual FileSystem& files() noexcept = 0;
    [[nodiscard]] virtual Threading& threads() noexcept = 0;
    [[nodiscard]] virtual Network& network() noexcept = 0;
    [[nodiscard]] virtual Ipc& ipc() noexcept = 0;
};

// The Host of the platform this program was built for. Defined once per
// platform (src/platforms/<name>/host_<name>.cpp); the selector chose which.
// This is where an operating system's one-time start-up lives (Winsock on
// Windows), and the Host's destructor undoes it, so nothing happens before
// main and nothing is left running after it. Call it once; the services it
// returns live as long as the Host.
[[nodiscard]] Result<std::unique_ptr<Host>> make_host() noexcept;

}  // namespace adl::platform
