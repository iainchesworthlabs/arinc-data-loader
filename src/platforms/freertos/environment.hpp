#pragma once

// What a FreeRTOS product tells the platform layer before it asks for a Host.
//
// FreeRTOS is a kernel, not an operating system: the network stack, the file system and
// the task plan are the product's choices, and this port is written against one set of
// them (docs/platform-abstraction.md): lwIP's sockets API for UDP and littlefs for files.
// The product starts both, as it starts anything else, and hands the file system over here.
// The clock, threads, memory and queues need nothing from the product but the kernel.
//
// This header names no FreeRTOS, lwIP or littlefs header; `lfs` is littlefs's own struct
// (`lfs_t` is a typedef of it).

// littlefs's file system object, `lfs_t` (declared here so that this header needs no
// littlefs header).
struct lfs;

namespace adl::platform::freertos {

struct Environment {
    // A mounted littlefs (lfs_mount has succeeded), which must outlive every Host made after
    // this call and must have been built thread-safe (LFS_THREADSAFE, with the lock and
    // unlock of its configuration), because the runner, the application and the engine's
    // sinks use it from different tasks. Null leaves every file call failing with
    // `unsupported`.
    ::lfs* file_system = nullptr;

    // The priority of the network stack's task (lwIP's tcpip_thread). The threads the loader
    // starts are given priorities below it: a runner above the stack's task would wait for the
    // thread that has to hand it its packets. 0 means "one below configMAX_PRIORITIES - 1",
    // which is where FreeRTOS+TCP and the lwIP port put theirs by default.
    unsigned network_task_priority = 0;
};

// Replaces the environment make_host() reads. Call it once, before make_host(); hosts made
// earlier keep the one they were made with.
void set_environment(const Environment& environment) noexcept;

}  // namespace adl::platform::freertos
