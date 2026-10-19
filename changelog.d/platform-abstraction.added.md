**The platform abstraction for the ARINC 615A loader**
([docs/platform-abstraction.md](docs/platform-abstraction.md)): interfaces for
files and storage, memory (an arena, a block pool and a provider, chosen over
`std::pmr`), threads with an abstract priority scheme, mutexes and semaphores,
a monotonic clock, IPv4 UDP sockets that wait on several at once, and bounded
message queues. Windows, Linux and macOS implement them (Winsock and Win32;
POSIX, with the differences of Linux and macOS in one small file each). Failure
is `std::expected`, nothing throws, and a platform with no heap can serve the
handles from fixed slots.
