// SPDX-License-Identifier: GPL-3.0-or-later
// `adl_load serve`: the loop that takes requests, starts operations on the loader library's pool, and writes
// what happens. Everything here runs on one thread: the requests are read from the source without waiting, the
// pool is stepped for a short time between them, and an operation's events are written from its completion, from
// the pool's step. So every event is a whole line written by itself.

#include "adl/load/serve.hpp"

#include "adl/load/events.hpp"
#include "adl/load/exit_codes.hpp"
#include "adl/loader_core/address.hpp"
#include "adl/loader_core/operation_pool.hpp"
#include "operation_command.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace adl::load
{

namespace
{

// How long the loop waits on the network between looks at the requests. A request is taken at most this late.
constexpr std::chrono::milliseconds pass_length{20};

// One operation that was started and has not finished: its options, the writer that puts its id on each event,
// and the steps of the operation. Not moved once made, since the parts point at one another.
struct Served
{
    Served(std::string operation_id, Options operation_options, adl::platform::Host& host, std::ostream& out,
           adl::platform::Instant start)
        : id(std::move(operation_id)), options(std::move(operation_options)), events(out, true, host.clock(), start),
          command(host, options, events)
    {
        events.set_id(id);
    }
    Served(const Served&) = delete;
    Served& operator=(const Served&) = delete;

    std::string id;
    Options options;
    EventWriter events;
    OperationCommand command;
};

class Server
{
public:
    Server(adl::platform::Host& host, const Options& options, LineSource& input, std::ostream& out, std::ostream& err,
           adl::platform::Instant start)
        : host_(host), options_(options), input_(input), out_(out), err_(err), start_(start),
          events_(out, true, host.clock(), start), pool_(host, pool_config(options))
    {
    }

    [[nodiscard]] int run()
    {
        if (const auto opened = pool_.open(); !opened)
        {
            err_ << "adl_load: the loader's port could not be opened (" << adl::platform::to_string(opened.error().code)
                 << ").\n";
            return exit_local_error;
        }
        if (const auto reading = input_.start(); !reading)
        {
            err_ << "adl_load: the requests could not be read (" << adl::platform::to_string(reading.error().code)
                 << ").\n";
            return exit_local_error;
        }
        const std::string address =
            options_.local_address ? adl::loader_core::format_ipv4(*options_.local_address) : std::string{"0.0.0.0"};
        events_.serve_started(address, pool_.control_port(), options_.max_operations);

        for (;;)
        {
            take_requests();
            if (events_.failed())
            {
                return exit_local_error;
            }
            if (!stopping_ && input_.ended())
            {
                stopping_ = true;
                reason_ = "end_of_input";
            }
            if (stopping_ && served_.empty())
            {
                break;
            }
            pool_.step(pass_length);
            reap();
            if (events_.failed())
            {
                return exit_local_error;
            }
        }
        events_.stopped(reason_, started_);
        return events_.failed() ? exit_local_error : exit_completed;
    }

private:
    [[nodiscard]] static adl::loader_core::PoolConfig pool_config(const Options& options)
    {
        adl::loader_core::PoolConfig config;
        config.control_port = options.control_port;
        if (options.local_address)
        {
            config.local_address = adl::platform::Ipv4Address{*options.local_address};
        }
        config.max_operations = options.max_operations;
        return config;
    }

    void take_requests()
    {
        std::vector<InputLine> lines;
        input_.poll(host_.clock().now(), lines);
        for (const InputLine& line : lines)
        {
            ++line_number_;
            handle(line);
        }
    }

    void refuse(const std::optional<std::string>& id, std::string_view detail)
    {
        events_.request_refused(id, line_number_, detail);
    }

    void handle(const InputLine& line)
    {
        if (line.end == LineEnd::too_long)
        {
            refuse(std::nullopt, "The line is longer than " + std::to_string(max_request_line_bytes) + " bytes.");
            return;
        }
        if (line.end == LineEnd::unfinished)
        {
            refuse(std::nullopt, "The input ended in the middle of a line, which is not a request.");
            return;
        }
        auto request = parse_request(line.text);
        if (!request)
        {
            refuse(request.error().id, request.error().detail);
            return;
        }
        const std::optional<std::string> id =
            request->kind == RequestKind::stop ? std::nullopt : std::optional<std::string>{request->id};
        if (stopping_)
        {
            refuse(id, "The program has been told to stop and takes no more requests.");
            return;
        }
        switch (request->kind)
        {
        case RequestKind::stop:
            stopping_ = true;
            reason_ = "stop";
            return;
        case RequestKind::cancel:
            cancel(*request);
            return;
        case RequestKind::run:
            run_request(std::move(*request));
            return;
        }
    }

    void cancel(const Request& request)
    {
        const auto found = served_.find(request.id);
        if (found == served_.end())
        {
            refuse(request.id, "No operation with this id is running.");
            return;
        }
        // An operation that could not be started is already ending and has nothing to abort.
        static_cast<void>(pool_.abort(adl::arinc615a::Address{found->second->options.target}));
    }

    void run_request(Request request)
    {
        const std::string id = request.id;
        if (served_.contains(id))
        {
            refuse(id, "An operation with this id has not finished.");
            return;
        }
        const adl::arinc615a::Address address{request.options.target};
        for (const auto& [other_id, other] : served_)
        {
            if (adl::arinc615a::Address{other->options.target} == address)
            {
                refuse(id, "The target " + adl::loader_core::format_ipv4(request.options.target) +
                               " has an operation that has not finished (id " + other_id + ").");
                return;
            }
        }
        if (served_.size() >= options_.max_operations)
        {
            refuse(id, "The program runs " + std::to_string(options_.max_operations) +
                           " operations at once, and that many have not finished.");
            return;
        }

        auto served = std::make_unique<Served>(id, std::move(request.options), host_, out_, start_);
        // Wrong usage that only shows once the media set is read is told as a refusal, as a command line tells it
        // as wrong usage: nothing of the operation has been written.
        if (const std::optional<std::string> problem = served->command.prepare())
        {
            refuse(id, *problem);
            return;
        }
        ++started_;
        if (served->command.begin())
        {
            return; // it ended there (the set could not be read, the folder could not be made), and has said so
        }
        served->command.announce();

        adl::loader_core::PoolTarget target;
        target.address = address;
        target.thw_id_pos = served->options.position;
        target.port = served->options.target_port;
        Served* const raw = served.get();
        const auto completion = [this, raw](adl::loader_core::OperationResult&& result)
        {
            static_cast<void>(raw->command.end(result));
            finished_.push_back(raw->id);
        };
        served_.emplace(id, std::move(served));
        if (const auto started = pool_.start(target, raw->command.operation(), raw->command.services(), completion);
            !started)
        {
            // The checks above leave nothing for the pool to refuse. If it does, the operation ends as one that
            // could not run, which is what it is.
            adl::loader_core::OperationResult none;
            none.not_run = adl::arinc615a::RunError{};
            static_cast<void>(raw->command.end(none));
            served_.erase(id);
        }
    }

    // Takes out the operations that have finished, now that the pool has done with them.
    void reap()
    {
        for (const std::string& id : finished_)
        {
            served_.erase(id);
        }
        finished_.clear();
    }

    adl::platform::Host& host_;
    const Options& options_;
    LineSource& input_;
    std::ostream& out_;
    std::ostream& err_;
    adl::platform::Instant start_;
    EventWriter events_;
    adl::loader_core::OperationPool pool_;
    std::map<std::string, std::unique_ptr<Served>> served_;
    std::vector<std::string> finished_;
    std::uint64_t line_number_ = 0;
    std::uint64_t started_ = 0;
    bool stopping_ = false;
    std::string_view reason_ = "stop";
};

} // namespace

int run_serve(adl::platform::Host& host, const Options& options, LineSource& input, std::ostream& out,
              std::ostream& err, adl::platform::Instant start)
{
    return Server(host, options, input, out, err, start).run();
}

} // namespace adl::load
