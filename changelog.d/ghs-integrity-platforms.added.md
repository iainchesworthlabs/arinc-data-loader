**The two Green Hills INTEGRITY platforms, `ghs508` and `ghs178b`**
(unbuilt): a port of the platform layer to INTEGRITY 5.0.8 on the shared
POSIX sources, and a native port to INTEGRITY-178B (semaphores, the
virtual-clock time-out pattern, ipflite UDP, a static memory provider), with
`cmake/FindIntegrity.cmake`, `cmake/FindIntegrity178.cmake`, the GHS
toolchain files and presets. The interfaces stay C++23; a GHS standard
library that is short one of the C++20/23 headers the interfaces include
gets a shim from `src/platforms/ghs/std/`, selected automatically per header
by `cmake/GhsStdShims.cmake`.
[docs/platform-ghs.md](docs/platform-ghs.md) is the review that shaped them,
and names what is still open (the 178B application task and file-system
model).
