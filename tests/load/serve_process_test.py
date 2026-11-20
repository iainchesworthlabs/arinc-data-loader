# SPDX-License-Identifier: GPL-3.0-or-later
"""adl_load serve started as a process with its standard input and output as pipes, which no test on the fake
platform can be: what standard input does when it is a pipe that stays open, bytes that arrive in UTF-8 and have
to reach the program as they are, and a process that ends by itself when it is told to stop.

    python3 serve_process_test.py <adl_load program> <work directory>

Nothing needs a target: an operation is aborted by a cancel before the target has answered anything, and a
request is refused before it starts. The control port is a free high one, found by binding it and letting it go.
Exit code 0 when everything held, 1 with a message for each thing that did not."""

import json
import os
import socket
import subprocess
import sys
import threading

PROGRAM = sys.argv[1]
WORK = sys.argv[2]
PROBLEMS = []


def check(condition, message):
    if not condition:
        PROBLEMS.append(message)
        print("FAILED: " + message, file=sys.stderr)


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class Serve:
    """A running `adl_load serve`, with a watchdog that ends it if a check below waits for what never comes."""

    def __init__(self, *options):
        self.process = subprocess.Popen(
            [PROGRAM, "serve", *options],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.watchdog = threading.Timer(90, self.process.kill)
        self.watchdog.start()

    def send(self, line):
        self.process.stdin.write(line.encode("utf-8") + b"\n")
        self.process.stdin.flush()

    def event(self):
        raw = self.process.stdout.readline()
        if not raw:
            return None
        return json.loads(raw.decode("utf-8"))

    def events_until(self, name):
        """The events up to and including the first called `name` (or to the end of the output)."""
        seen = []
        while True:
            event = self.event()
            if event is None:
                return seen
            seen.append(event)
            if event["event"] == name:
                return seen

    def finish(self, timeout=20):
        """Waits for the process to end by itself, whatever its standard input is doing."""
        try:
            code = self.process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            code = None
        self.watchdog.cancel()
        rest = self.process.stdout.read().decode("utf-8")
        err = self.process.stderr.read().decode("utf-8")
        return code, [json.loads(line) for line in rest.splitlines() if line.strip()], err


def operation_request(identifier, record, port):
    fields = ["run", identifier, "information", "--target", "127.0.0.1", "--position", "HNPFMS_L",
              "--target-port", str(port), "--record", record]
    return "\t".join(fields)


def test_wrong_usage():
    process = subprocess.run([PROGRAM, "serve", "--max-operations", "0"], capture_output=True, input=b"", timeout=30)
    check(process.returncode == 2, "serve --max-operations 0 should end with code 2, not %r" % process.returncode)
    check(process.stdout == b"", "wrong usage should write no event")
    check(b"--max-operations takes a whole number from 1 to 64" in process.stderr, "the sentence of wrong usage")


def test_port_held():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as holder:
        holder.bind(("127.0.0.1", 0))
        port = holder.getsockname()[1]
        process = subprocess.run(
            [PROGRAM, "serve", "--local-address", "127.0.0.1", "--control-port", str(port)],
            capture_output=True, input=b"stop\n", timeout=30,
        )
    check(process.returncode == 5, "a port that is held should end with code 5, not %r" % process.returncode)
    check(process.stdout == b"", "a port that is held writes no event: started means the port is held")
    check(b"the loader's port could not be opened" in process.stderr, "the sentence for a port that is held")


def test_stop_with_standard_input_left_open():
    serve = Serve("--local-address", "127.0.0.1", "--control-port", str(free_port()))
    first = serve.event()
    check(first is not None and first["event"] == "started" and first["command"] == "serve", "started is first")
    serve.send("stop")
    # Standard input stays open: the program has to end because it was told to, not because the input did.
    code, rest, err = serve.finish()
    check(code == 0, "stop should end the process with code 0, not %r (stderr: %s)" % (code, err))
    check(rest and rest[-1]["event"] == "stopped" and rest[-1]["reason"] == "stop", "stopped says why: %r" % rest)
    serve.process.stdin.close()


def test_end_of_input():
    serve = Serve("--local-address", "127.0.0.1", "--control-port", str(free_port()), "--max-operations", "2")
    serve.process.stdin.close()
    first = serve.event()
    check(first is not None and first["max_operations"] == 2, "started carries the limit: %r" % first)
    code, rest, _ = serve.finish()
    check(code == 0, "the end of the input should end the process with code 0, not %r" % code)
    check(rest and rest[-1]["reason"] == "end_of_input", "stopped says the input ended: %r" % rest)


def test_refusal_and_cancel_with_a_path_in_any_script():
    folder = os.path.join(WORK, "serve-é-日本語-\U0001F600 and spaces")
    os.makedirs(folder, exist_ok=True)
    record = os.path.join(folder, "record é.json")
    serve = Serve("--local-address", "127.0.0.1", "--control-port", str(free_port()))
    serve.event()  # started

    serve.send("pause")
    refused = serve.event()
    check(refused is not None and refused["event"] == "request_refused" and refused["line"] == 1
          and refused["id"] is None, "an unknown request is refused: %r" % refused)

    serve.send(operation_request("a.1", record, free_port()))
    serve.send("cancel\ta.1")
    events = serve.events_until("finished")
    names = [event["event"] for event in events]
    check(names and names[0] == "started" and names[-1] == "finished", "the operation's events: %r" % names)
    check(all(event.get("id") == "a.1" for event in events), "each event carries the id: %r" % events)
    finished = events[-1]
    check(finished["reason"] == "aborted" and finished["exit_code"] == 1, "a cancel aborts: %r" % finished)
    # The path came through standard input as UTF-8, whatever the code page, and the record is where it says.
    check(finished.get("record") == record, "the record path is as it was sent: %r" % finished.get("record"))
    check(os.path.isfile(record), "the record is in the folder that has characters outside the code page")

    serve.send("stop")
    code, rest, err = serve.finish()
    check(code == 0, "the process ends with code 0, not %r (stderr: %s)" % (code, err))
    check(rest and rest[-1]["event"] == "stopped" and rest[-1]["operations"] == 1, "stopped: %r" % rest)
    serve.process.stdin.close()


def main():
    os.makedirs(WORK, exist_ok=True)
    test_wrong_usage()
    test_port_held()
    test_stop_with_standard_input_left_open()
    test_end_of_input()
    test_refusal_and_cancel_with_a_path_in_any_script()
    if PROBLEMS:
        return 1
    print("adl_load serve as a process: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
