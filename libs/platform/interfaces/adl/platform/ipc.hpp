#pragma once

// Passing messages between the threads of one process: from the runner thread
// to the application (progress, a received status file, the end of an
// operation) and back (an abort request, a file selection). A bounded queue
// of messages, so a slow reader makes a fast writer wait or fail instead of
// the program's memory growing. Signalling with nothing to carry is a
// Semaphore (threads.hpp).
//
// There is no inter-process IPC here: nothing in 615A needs it, and the
// protocols between processes (QNX messages, VxWorks message queues shared
// between tasks of different protection domains) differ too much to hide behind
// one interface until a use for it exists.

#include "adl/platform/error.hpp"
#include "adl/platform/owned.hpp"
#include "adl/platform/time.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace adl::platform
{

struct QueueSpec
{
    std::size_t message_bytes = 0; // the largest message; each may be shorter
    std::size_t capacity = 0;      // how many messages the queue holds
    // Where the queue keeps its messages, for a platform that must not
    // allocate. Of at least storage_required() bytes; empty lets the platform
    // allocate (which a desktop does, once, in create_queue).
    std::span<std::byte> storage{};

    // Enough for the messages and the length each one carries.
    [[nodiscard]] static constexpr std::size_t storage_required(std::size_t message_bytes,
                                                                std::size_t capacity) noexcept
    {
        return capacity * (message_bytes + sizeof(std::size_t) + alignof(std::size_t));
    }
};

// Many threads may send and many may receive. Messages arrive whole, in the
// order they were accepted.
class MessageQueue
{
public:
    virtual ~MessageQueue() = default;

    // Copies `message` into the queue, waiting up to `timeout` for room
    // (`Duration::zero()` does not wait). `timeout` if it stayed full,
    // `invalid_argument` if the message is longer than message_bytes().
    [[nodiscard]] virtual Result<void> send(std::span<const std::byte> message, Duration timeout) noexcept = 0;

    // Copies the oldest message into `out` and returns its length, waiting up
    // to `timeout` for one. `invalid_argument` if `out` is shorter than
    // message_bytes() (so a message can never be cut short), `timeout` if
    // none arrived.
    [[nodiscard]] virtual Result<std::size_t> receive(std::span<std::byte> out, Duration timeout) noexcept = 0;

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual std::size_t capacity() const noexcept = 0;
    [[nodiscard]] virtual std::size_t message_bytes() const noexcept = 0;
};

class Ipc
{
public:
    virtual ~Ipc() = default;

    // `invalid_argument` for a zero size or capacity, or for storage that is too
    // small.
    [[nodiscard]] virtual Result<Owned<MessageQueue>> create_queue(const QueueSpec& spec) noexcept = 0;
};

} // namespace adl::platform
