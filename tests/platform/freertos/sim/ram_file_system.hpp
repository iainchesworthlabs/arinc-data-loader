#pragma once

// littlefs on a block device that is an array in RAM, for the simulator program: the file system
// the platform layer's FileSystem works on. The mutex-backed lock is what LFS_THREADSAFE
// asks of the configuration, because the runner, the application and the engine's file sinks
// use the file system from different tasks.

#include <FreeRTOS.h>
#include <lfs.h>
#include <semphr.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace adl::test
{

class RamFileSystem
{
public:
    // `block_size` * `block_count` bytes of "flash". Formats it and mounts it.
    RamFileSystem(std::size_t block_size, std::size_t block_count);
    ~RamFileSystem();

    RamFileSystem(const RamFileSystem&) = delete;
    RamFileSystem& operator=(const RamFileSystem&) = delete;

    // Whether formatting and mounting worked.
    [[nodiscard]] bool ok() const noexcept
    {
        return mounted_;
    }
    [[nodiscard]] lfs_t* get() noexcept
    {
        return &lfs_;
    }

private:
    static int read(const lfs_config* config, lfs_block_t block, lfs_off_t offset, void* buffer, lfs_size_t size);
    static int program(const lfs_config* config, lfs_block_t block, lfs_off_t offset, const void* buffer,
                       lfs_size_t size);
    static int erase(const lfs_config* config, lfs_block_t block);
    static int sync(const lfs_config* config);
    static int lock(const lfs_config* config);
    static int unlock(const lfs_config* config);

    std::vector<std::uint8_t> flash_;
    SemaphoreHandle_t mutex_ = nullptr;
    lfs_config config_{};
    lfs_t lfs_{};
    bool mounted_ = false;
};

} // namespace adl::test
