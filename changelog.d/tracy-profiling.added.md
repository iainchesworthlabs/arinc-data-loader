**Tracy 0.14.1 profiling** ([docs/profiling.md](docs/profiling.md)): an opt-in build
(`-DADL_ENABLE_TRACY=ON`, vcpkg's `profiling` feature) instruments ARINC 665 and the media tree,
the 615A engine and runner, the application's engine jobs and media-set reads, the operator's
actions, and the window's frame loop — frame marks, the frame interval and the render thread's
time as plots, with the GUI, engine and render threads named. The `adl::profiling` names come
from a header CMake generates, so no first-party file carries `#if` or `#define` and the same
sources compile to nothing when the switch is off. The feature also installs Tracy's CLI tools,
so a trace can be recorded and exported without the GUI profiler.
