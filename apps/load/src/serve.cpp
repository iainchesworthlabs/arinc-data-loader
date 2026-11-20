// SPDX-License-Identifier: GPL-3.0-or-later
// `adl_load serve`: the loop that takes requests, starts operations on the loader library's pool, and writes
// what happens. Everything here runs on one thread: the requests are read from the source without waiting, the
// pool is stepped for a short time between them, and an operation's events are written from its completion, from
// the pool's step. So every event is a whole line written by itself.
//
// What an operation needs read before it can start (an upload's media set, which can take longer than a target
// waits) is not done here: it is handed to a Preparer, which runs it on a thread of its own, and the operation goes
// on when the answer comes back. Until then nothing is written for its id, which is in use, and its address is taken.

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

// One operation that was taken and has not finished: its options, the writer that puts its id on each event, and the
// steps of the operation. Not moved once made, since the parts point at one another. It is shared with the
// preparation, which runs on another thread and may outlast the loop that was told to give up (the output could not
// be written), so it lives as long as the work that uses it.
struct Served
{
    Served(std::string operation_id, Options operation_options, adl::platform::Host& host, std::ostream& out,
           adl::platform::Instant start, std::uint64_t input_line, LicenceSource licence)
        : id(std::move(operation_id)), line(input_line), options(std::move(operation_options)),
          events(out, true, host.clock(), start), command(host, options, events, std::move(licence))
    {
        events.set_id(id);
    }
    Served(const Served&) = delete;
    Served& operator=(const Served&) = delete;

    std::string id;
    std::uint64_t line; // of the request that asked for it
    Options options;
    EventWriter events;
    OperationCommand command;
    // Its media set is being read, away from the loop: nothing has been written for it. Only the loop changes these,
    // and the preparation touches `command` and nothing else.
    bool preparing = false;
    bool cancelled = false; // a `cancel` came while it was being prepared
};

class Server
{
public:
    Server(adl::platform::Host& host, const Options& options, LineSource& input, Preparer& preparer, std::ostream& out,
           std::ostream& err, adl::platform::Instant start, const LicenceSource& licence)
        : host_(host), options_(options), input_(input), preparer_(preparer), out_(out), err_(err), start_(start),
          licence_(licence), events_(out, true, host.clock(), start), pool_(host, pool_config(options))
    {
    }

    [[nodiscard]] int run()
    {
        if (const auto opened = pool_.open(); !opened)
        {
            err_ << "adl_load: the loader's port could not be opened (" << adl::platform::to_string(opened.error().code)
                 << ").";
            if (opened.error().code == adl::platform::ErrorCode::address_in_use)
            {
                // Listening on every interface is refused when anything has the port on one of them, simulated targets
                // on port 59 included, and listening on one interface is not.
                err_ << " Give --local-address <ipv4> to listen on one interface only.";
            }
            err_ << '\n';
            return exit_local_error;
        }
        if (const auto preparing = preparer_.start(); !preparing)
        {
            err_ << "adl_load: the media sets could not be prepared ("
                 << adl::platform::to_string(preparing.error().code) << ").\n";
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
            take_prepared();
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

    // A request of the line being handled that is not done.
    void refuse(const std::optional<std::string>& id, const std::optional<std::string>& request,
                std::string_view detail)
    {
        events_.request_refused(id, request, line_number_, detail);
    }

    void handle(const InputLine& line)
    {
        if (line.end == LineEnd::too_long)
        {
            refuse(std::nullopt, std::nullopt,
                   "The line is longer than " + std::to_string(max_request_line_bytes) + " bytes.");
            return;
        }
        if (line.end == LineEnd::unfinished)
        {
            refuse(std::nullopt, std::nullopt, "The input ended in the middle of a line, which is not a request.");
            return;
        }
        auto request = parse_request(line.text);
        if (!request)
        {
            refuse(request.error().id, request.error().request, request.error().detail);
            return;
        }
        switch (request->kind)
        {
        case RequestKind::stop:
            // A second stop changes nothing.
            if (!stopping_)
            {
                stopping_ = true;
                reason_ = "stop";
            }
            return;
        case RequestKind::cancel:
            // Still taken after a stop: a caller that is told to end has to be able to end what is running.
            cancel(*request);
            return;
        case RequestKind::run:
            if (stopping_)
            {
                refuse(request->id, "run", "The program has been told to stop and starts no more operations.");
                return;
            }
            run_request(std::move(*request));
            return;
        }
    }

    void cancel(const Request& request)
    {
        const auto found = served_.find(request.id);
        if (found == served_.end())
        {
            refuse(request.id, "cancel", "No operation with this id is running.");
            return;
        }
        if (found->second->preparing)
        {
            // Its set is still being read. The operation is started when it has been, and ends as soon as it has, with
            // the reason a cancel gives, so that the caller meets the same events as for a cancel a moment later.
            found->second->cancelled = true;
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
            refuse(id, "run", "An operation with this id has not finished.");
            return;
        }
        const adl::arinc615a::Address address{request.options.target};
        for (const auto& [other_id, other] : served_)
        {
            if (adl::arinc615a::Address{other->options.target} == address)
            {
                refuse(id, "run",
                       "The target " + adl::loader_core::format_ipv4(request.options.target) +
                           " has an operation that has not finished (id " + other_id + ").");
                return;
            }
        }
        if (served_.size() >= options_.max_operations)
        {
            refuse(id, "run",
                   "The program runs " + std::to_string(options_.max_operations) +
                       " operations at once, and that many have not finished.");
            return;
        }

        const bool reads_a_set = request.options.command == Command::upload;
        auto served =
            std::make_shared<Served>(id, std::move(request.options), host_, out_, start_, line_number_, licence_);
        served_.emplace(id, served);
        if (reads_a_set)
        {
            // The media set is read away from here, one request at a time, and the answer comes back by
            // take_prepared(). Nothing is written for the id until then, and it and its address are in use.
            served->preparing = true;
            const std::uint64_t ticket = ++last_ticket_;
            tickets_.emplace(ticket, id);
            preparer_.submit(ticket, [served] { return served->command.prepare(); });
            return;
        }
        // What can be wrong with the choice is found before anything is written: a refusal, as a command line tells it
        // as wrong usage.
        if (const std::optional<std::string> problem = served->command.prepare())
        {
            refuse(id, "run", *problem);
            served_.erase(id);
            return;
        }
        launch(served);
    }

    // The preparations that have ended: each operation goes on, or is refused for what the preparation found.
    void take_prepared()
    {
        std::vector<Preparation> done;
        preparer_.poll(host_.clock().now(), done);
        for (Preparation& preparation : done)
        {
            const auto ticket = tickets_.find(preparation.ticket);
            if (ticket == tickets_.end())
            {
                continue;
            }
            const std::string id = ticket->second;
            tickets_.erase(ticket);
            const auto found = served_.find(id);
            if (found == served_.end())
            {
                continue;
            }
            const std::shared_ptr<Served> served = found->second;
            served->preparing = false;
            if (preparation.problem)
            {
                // Wrong usage that only the media set shows (a part number it does not hold): the run is refused, with
                // the number of the line it was on, and nothing of it has been written.
                events_.request_refused(id, "run", served->line, *preparation.problem);
                served_.erase(id);
                continue;
            }
            launch(served);
        }
    }

    // Writes `started` and what follows it until the engine is needed, and gives the operation to the pool.
    void launch(const std::shared_ptr<Served>& served)
    {
        ++started_;
        if (served->command.begin())
        {
            // It ended there (the set could not be read, the folder could not be made), and has said so.
            served_.erase(served->id);
            return;
        }
        served->command.announce();
        if (served->cancelled)
        {
            // Cancelled while its set was being read: it ends as an operation aborted at once would.
            adl::loader_core::OperationResult aborted;
            aborted.reason = adl::loader_core::Reason::aborted;
            aborted.outcome.code = adl::arinc615a::OutcomeCode::aborted_by_operator;
            static_cast<void>(served->command.end(aborted));
            served_.erase(served->id);
            return;
        }

        adl::loader_core::PoolTarget target;
        target.address = adl::arinc615a::Address{served->options.target};
        target.thw_id_pos = served->options.position;
        target.port = served->options.target_port;
        Served* const raw = served.get();
        const auto completion = [this, raw](adl::loader_core::OperationResult&& result)
        {
            static_cast<void>(raw->command.end(result));
            finished_.push_back(raw->id);
        };
        if (const auto started =
                pool_.start(target, served->command.operation(), served->command.services(), completion);
            !started)
        {
            // The checks made when the request came leave nothing for the pool to refuse. If it does, the operation
            // ends as one that could not run, which is what it is.
            adl::loader_core::OperationResult none;
            none.not_run = adl::arinc615a::RunError{};
            static_cast<void>(served->command.end(none));
            served_.erase(served->id);
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
    Preparer& preparer_;
    std::ostream& out_;
    std::ostream& err_;
    adl::platform::Instant start_;
    LicenceSource licence_; // each operation gets a copy and reads the licence when it starts
    EventWriter events_;
    adl::loader_core::OperationPool pool_;
    std::map<std::string, std::shared_ptr<Served>> served_;
    std::map<std::uint64_t, std::string> tickets_; // the preparations that have not ended, by ticket
    std::vector<std::string> finished_;
    std::uint64_t last_ticket_ = 0;
    std::uint64_t line_number_ = 0;
    std::uint64_t started_ = 0;
    bool stopping_ = false;
    std::string_view reason_ = "stop";
};

} // namespace

int run_serve(adl::platform::Host& host, const Options& options, LineSource& input, Preparer& preparer,
              std::ostream& out, std::ostream& err, adl::platform::Instant start, const LicenceSource& licence)
{
    return Server(host, options, input, preparer, out, err, start, licence).run();
}

} // namespace adl::load
