// SPDX-License-Identifier: GPL-3.0-or-later
#include "ram_file_system.hpp"

#include <cstring>

namespace adl::test
{

RamFileSystem::RamFileSystem(std::size_t block_size, std::size_t block_count)
    : flash_(block_size * block_count, std::uint8_t{0xFF}), mutex_(xSemaphoreCreateRecursiveMutex())
{
    config_.context = this;
    config_.read = &RamFileSystem::read;
    config_.prog = &RamFileSystem::program;
    config_.erase = &RamFileSystem::erase;
    config_.sync = &RamFileSystem::sync;
    config_.lock = &RamFileSystem::lock;
    config_.unlock = &RamFileSystem::unlock;
    config_.read_size = 16;
    config_.prog_size = 16;
    config_.block_size = static_cast<lfs_size_t>(block_size);
    config_.block_count = static_cast<lfs_size_t>(block_count);
    config_.cache_size = 512;
    config_.lookahead_size = 128;
    config_.block_cycles = 500;

    mounted_ = mutex_ != nullptr && lfs_format(&lfs_, &config_) == 0 && lfs_mount(&lfs_, &config_) == 0;
}

RamFileSystem::~RamFileSystem()
{
    if (mounted_)
    {
        (void)lfs_unmount(&lfs_);
    }
    if (mutex_ != nullptr)
    {
        vSemaphoreDelete(mutex_);
    }
}

int RamFileSystem::read(const lfs_config* config, lfs_block_t block, lfs_off_t offset, void* buffer, lfs_size_t size)
{
    auto* self = static_cast<RamFileSystem*>(config->context);
    std::memcpy(buffer, self->flash_.data() + block * config->block_size + offset, size);
    return 0;
}

int RamFileSystem::program(const lfs_config* config, lfs_block_t block, lfs_off_t offset, const void* buffer,
                           lfs_size_t size)
{
    auto* self = static_cast<RamFileSystem*>(config->context);
    std::memcpy(self->flash_.data() + block * config->block_size + offset, buffer, size);
    return 0;
}

int RamFileSystem::erase(const lfs_config* config, lfs_block_t block)
{
    auto* self = static_cast<RamFileSystem*>(config->context);
    std::memset(self->flash_.data() + block * config->block_size, 0xFF, config->block_size);
    return 0;
}

int RamFileSystem::sync(const lfs_config* /*config*/)
{
    return 0;
}

int RamFileSystem::lock(const lfs_config* config)
{
    auto* self = static_cast<RamFileSystem*>(config->context);
    while (xSemaphoreTakeRecursive(self->mutex_, portMAX_DELAY) != pdTRUE)
    {
    }
    return 0;
}

int RamFileSystem::unlock(const lfs_config* config)
{
    auto* self = static_cast<RamFileSystem*>(config->context);
    (void)xSemaphoreGiveRecursive(self->mutex_);
    return 0;
}

} // namespace adl::test
