#include "fake_storage.hpp"

#include <algorithm>
#include <cstring>
#include <new>

namespace adl::fake {
namespace {

using platform::ErrorCode;

}  // namespace

// ---- MemoryFileSystem -------------------------------------------------------

class MemoryReadFile final : public platform::ReadFile {
public:
    explicit MemoryReadFile(std::shared_ptr<std::vector<std::byte>> bytes) noexcept
        : bytes_(std::move(bytes)) {}

    platform::Result<std::uint64_t> size() noexcept override { return bytes_->size(); }

    platform::Result<std::size_t> read_at(std::uint64_t offset,
                                          std::span<std::byte> out) noexcept override {
        if (offset >= bytes_->size()) {
            return std::size_t{0};
        }
        const std::size_t count =
            std::min<std::size_t>(out.size(), bytes_->size() - static_cast<std::size_t>(offset));
        if (count != 0) {
            std::memcpy(out.data(), bytes_->data() + offset, count);
        }
        return count;
    }

private:
    std::shared_ptr<std::vector<std::byte>> bytes_;
};

class MemoryWriteFile final : public platform::WriteFile {
public:
    MemoryWriteFile(MemoryFileSystem& files, std::shared_ptr<MemoryFileSystem::Content> content)
        : files_(files), content_(std::move(content)) {}

    platform::Result<void> write(std::span<const std::byte> data) noexcept override {
        if (!content_) {
            return platform::fail(ErrorCode::closed);
        }
        if (files_.write_failure_) {
            const ErrorCode code = *files_.write_failure_;
            files_.write_failure_.reset();
            return platform::fail(code);
        }
        if (files_.used_bytes() + data.size() > files_.capacity_) {
            return platform::fail(ErrorCode::no_space);
        }
        content_->bytes.insert(content_->bytes.end(), data.begin(), data.end());
        written_ += data.size();
        return {};
    }

    platform::Result<std::uint64_t> size() noexcept override {
        if (!content_) {
            return platform::fail(ErrorCode::closed);
        }
        return written_;
    }

    platform::Result<void> sync() noexcept override {
        if (!content_) {
            return platform::fail(ErrorCode::closed);
        }
        return {};
    }

    platform::Result<void> close() noexcept override {
        if (!content_) {
            return platform::fail(ErrorCode::closed);
        }
        content_.reset();
        return {};
    }

private:
    MemoryFileSystem& files_;
    std::shared_ptr<MemoryFileSystem::Content> content_;
    std::uint64_t written_ = 0;
};

MemoryFileSystem::MemoryFileSystem() {
    nodes_["/"] = Node{true, nullptr};
}

platform::Result<std::string> MemoryFileSystem::normalise(std::string_view path) const {
    if (path.empty()) {
        return platform::fail(ErrorCode::invalid_argument);
    }
    for (const char c : path) {
        if (c == '\0' || c == '\\') {
            return platform::fail(ErrorCode::invalid_argument);
        }
    }
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view part = path.substr(start, end - start);
        if (part == "..") {
            if (parts.empty()) {
                return platform::fail(ErrorCode::invalid_argument);
            }
            parts.pop_back();
        } else if (!part.empty() && part != ".") {
            parts.emplace_back(part);
        }
        start = end + 1;
    }
    std::string result = "/";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            result += '/';
        }
        result += parts[i];
    }
    return result;
}

std::string MemoryFileSystem::parent_of(const std::string& path) {
    const std::size_t slash = path.rfind('/');
    return slash == 0 ? "/" : path.substr(0, slash);
}

platform::Result<platform::Owned<platform::ReadFile>> MemoryFileSystem::open_read(
    std::string_view path) noexcept {
    auto name = normalise(path);
    if (!name) {
        return std::unexpected(name.error());
    }
    const auto found = nodes_.find(*name);
    if (found == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (found->second.directory) {
        return platform::fail(ErrorCode::is_a_directory);
    }
    // A reader holds the file's bytes, not its name, so removing or replacing the
    // file afterwards does not disturb it, as on POSIX.
    auto bytes = std::shared_ptr<std::vector<std::byte>>(found->second.content,
                                                         &found->second.content->bytes);
    return platform::adopt<platform::ReadFile>(std::make_unique<MemoryReadFile>(std::move(bytes)));
}

platform::Result<platform::Owned<platform::WriteFile>> MemoryFileSystem::create(
    std::string_view path, platform::CreateMode mode) noexcept {
    auto name = normalise(path);
    if (!name) {
        return std::unexpected(name.error());
    }
    const auto parent = nodes_.find(parent_of(*name));
    if (*name == "/" || parent == nodes_.end()) {
        return platform::fail(*name == "/" ? ErrorCode::is_a_directory : ErrorCode::not_found);
    }
    if (!parent->second.directory) {
        return platform::fail(ErrorCode::not_a_directory);
    }
    auto existing = nodes_.find(*name);
    if (existing != nodes_.end()) {
        if (mode == platform::CreateMode::exclusive) {
            return platform::fail(ErrorCode::already_exists);
        }
        if (existing->second.directory) {
            return platform::fail(ErrorCode::is_a_directory);
        }
        existing->second.content->bytes.clear();
        return platform::adopt<platform::WriteFile>(
            std::make_unique<MemoryWriteFile>(*this, existing->second.content));
    }
    auto content = std::make_shared<Content>();
    nodes_[*name] = Node{false, content};
    return platform::adopt<platform::WriteFile>(std::make_unique<MemoryWriteFile>(*this, content));
}

platform::Result<platform::FileInfo> MemoryFileSystem::stat(std::string_view path) noexcept {
    auto name = normalise(path);
    if (!name) {
        return std::unexpected(name.error());
    }
    const auto found = nodes_.find(*name);
    if (found == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (found->second.directory) {
        return platform::FileInfo{platform::FileKind::directory, 0};
    }
    return platform::FileInfo{platform::FileKind::regular, found->second.content->bytes.size()};
}

platform::Result<void> MemoryFileSystem::make_directory(std::string_view path) noexcept {
    auto name = normalise(path);
    if (!name) {
        return std::unexpected(name.error());
    }
    if (nodes_.contains(*name)) {
        return platform::fail(ErrorCode::already_exists);
    }
    const auto parent = nodes_.find(parent_of(*name));
    if (parent == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (!parent->second.directory) {
        return platform::fail(ErrorCode::not_a_directory);
    }
    nodes_[*name] = Node{true, nullptr};
    return {};
}

platform::Result<void> MemoryFileSystem::remove(std::string_view path) noexcept {
    auto name = normalise(path);
    if (!name) {
        return std::unexpected(name.error());
    }
    const auto found = nodes_.find(*name);
    if (found == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (*name == "/") {
        return platform::fail(ErrorCode::permission_denied);
    }
    if (found->second.directory) {
        const bool has_children = std::any_of(nodes_.begin(), nodes_.end(), [&](const auto& entry) {
            return entry.first != "/" && parent_of(entry.first) == *name;
        });
        if (has_children) {
            return platform::fail(ErrorCode::directory_not_empty);
        }
    }
    nodes_.erase(found);
    return {};
}

platform::Result<void> MemoryFileSystem::rename(std::string_view from,
                                                std::string_view to) noexcept {
    auto source = normalise(from);
    if (!source) {
        return std::unexpected(source.error());
    }
    auto target = normalise(to);
    if (!target) {
        return std::unexpected(target.error());
    }
    const auto found = nodes_.find(*source);
    if (found == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (*source == "/" || *target == "/") {
        return platform::fail(ErrorCode::permission_denied);
    }
    const auto parent = nodes_.find(parent_of(*target));
    if (parent == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (!parent->second.directory) {
        return platform::fail(ErrorCode::not_a_directory);
    }
    if (*source == *target) {
        return {};
    }
    if (const auto existing = nodes_.find(*target); existing != nodes_.end()) {
        if (existing->second.directory || found->second.directory) {
            return platform::fail(existing->second.directory ? ErrorCode::is_a_directory
                                                             : ErrorCode::not_a_directory);
        }
        nodes_.erase(existing);
    }
    if (found->second.directory) {
        // Move the directory and everything below it.
        std::vector<std::pair<std::string, Node>> moved;
        const std::string prefix = *source + "/";
        for (auto it = nodes_.begin(); it != nodes_.end();) {
            if (it->first == *source || it->first.starts_with(prefix)) {
                moved.emplace_back(*target + it->first.substr(source->size()), it->second);
                it = nodes_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& [path, node] : moved) {
            nodes_[path] = std::move(node);
        }
    } else {
        Node node = found->second;
        nodes_.erase(found);
        nodes_[*target] = std::move(node);
    }
    return {};
}

platform::Result<void> MemoryFileSystem::list(std::string_view directory,
                                              platform::DirectoryVisitor visitor,
                                              void* context) noexcept {
    auto name = normalise(directory);
    if (!name) {
        return std::unexpected(name.error());
    }
    const auto found = nodes_.find(*name);
    if (found == nodes_.end()) {
        return platform::fail(ErrorCode::not_found);
    }
    if (!found->second.directory) {
        return platform::fail(ErrorCode::not_a_directory);
    }
    // Collect first, so that the visitor may change the file system.
    std::vector<std::pair<std::string, platform::FileKind>> entries;
    for (const auto& [path, node] : nodes_) {
        if (path != "/" && parent_of(path) == *name) {
            entries.emplace_back(
                path.substr(path.rfind('/') + 1),
                node.directory ? platform::FileKind::directory : platform::FileKind::regular);
        }
    }
    for (const auto& [entry, kind] : entries) {
        if (!visitor(context, entry, kind)) {
            break;
        }
    }
    return {};
}

void MemoryFileSystem::put(std::string_view path, std::string_view content) {
    std::vector<std::byte> bytes(content.size());
    if (!content.empty()) {
        std::memcpy(bytes.data(), content.data(), content.size());
    }
    put(path, bytes);
}

void MemoryFileSystem::put(std::string_view path, const std::vector<std::byte>& content) {
    auto name = normalise(path);
    if (!name) {
        return;
    }
    // Make the directories on the way.
    std::string directory;
    for (std::size_t slash = name->find('/', 1); slash != std::string::npos;
         slash = name->find('/', slash + 1)) {
        directory = name->substr(0, slash);
        nodes_.try_emplace(directory, Node{true, nullptr});
    }
    auto file = std::make_shared<Content>();
    file->bytes = content;
    nodes_[*name] = Node{false, file};
}

std::optional<std::vector<std::byte>> MemoryFileSystem::get(std::string_view path) const {
    auto name = normalise(path);
    if (!name) {
        return std::nullopt;
    }
    const auto found = nodes_.find(*name);
    if (found == nodes_.end() || found->second.directory) {
        return std::nullopt;
    }
    return found->second.content->bytes;
}

std::uint64_t MemoryFileSystem::used_bytes() const noexcept {
    std::uint64_t total = 0;
    for (const auto& [path, node] : nodes_) {
        if (!node.directory) {
            total += node.content->bytes.size();
        }
    }
    return total;
}

// ---- FakeMemory -------------------------------------------------------------

platform::Result<platform::MemoryRegion> FakeMemory::acquire(std::size_t bytes,
                                                             std::size_t alignment,
                                                             platform::MemoryUse use) noexcept {
    if (bytes == 0 || !platform::is_power_of_two(alignment)) {
        return platform::fail(ErrorCode::invalid_argument);
    }
    if (use == platform::MemoryUse::resident && deny_resident_) {
        return platform::fail(ErrorCode::resource_exhausted);
    }
    // Far beyond any machine: refuse here, as an allocator under a sanitizer aborts.
    constexpr std::size_t absurd = std::size_t{1} << 40;
    if (bytes > absurd || bytes > budget_ || outstanding_ > budget_ - bytes) {
        return platform::fail(ErrorCode::resource_exhausted);
    }
    // The alignment is part of the allocation's header: operator delete needs it back.
    const std::size_t header = std::max(alignment, alignof(std::max_align_t));
    void* raw = ::operator new(bytes + header, std::align_val_t(header), std::nothrow);
    if (raw == nullptr) {
        return platform::fail(ErrorCode::resource_exhausted);
    }
    auto* block = static_cast<std::byte*>(raw) + header;
    std::memset(block, 0, bytes);
    // The header holds the size of the whole allocation, just before the block.
    const std::size_t whole = bytes + header;
    std::memcpy(block - sizeof whole, &whole, sizeof whole);
    outstanding_ += bytes;
    return platform::MemoryRegion(*this, {block, bytes});
}

void FakeMemory::release(std::span<std::byte> bytes) noexcept {
    if (bytes.data() == nullptr) {
        return;
    }
    std::size_t whole = 0;
    std::memcpy(&whole, bytes.data() - sizeof whole, sizeof whole);
    const std::size_t header = whole - bytes.size();
    outstanding_ -= bytes.size();
    ::operator delete(bytes.data() - header, std::align_val_t(header));
}

}  // namespace adl::fake
