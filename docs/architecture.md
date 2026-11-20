# Architecture

## The shape

```text
            +--------------------------------------+
            |  application (Qt 6.12, apps/app)     |
            +------------------+-------------------+
                               |
                    +----------v-----------+
                    | adl::arinc615a_runner|  drives the engine over the platform
                    +-----+-----------+----+
                          |           |
          +---------------v--+   +----v-----------------+
          | adl::arinc615a   |   |  adl::platform       |
          | codec and engine |   |  libs/platform       |
          | (sans-I/O)       |   |                      |
          +--------+---------+   |  files, memory,      |
                   |             |  threads, clock,     |
          +--------v---------+   |  UDP, queues         |
          | adl::arinc665    |   +----------------------+
          | libs/arinc665    |
          +------------------+

   adl::media_tree (libs/media_tree): members <-> a directory tree, over adl::arinc665
   adl::loader_core (libs/loader_core): what the loader does without a window and without Qt (the media
   set on disk and its check, the folder a download is written into, the target's address, one operation
   made and run against a target, the record of an operation and the file it is written as), over the libraries above; the application calls it
   adl::set_maker (libs/set_maker): the media set generator without a window and without Qt (a plan, the
   making of a set, the reading of one into facts), over adl::loader_core; the generator's window and the
   command adl_make_set build on it
   adl::loader_scenario (libs/loader_scenario): faults on purpose in the loader's engine, from a scenario file
   (the scenario, its reader and the player of the engine's FaultHook); not installed
   adl::samples (libs/samples): the sample sets; adl_write_payloads writes them out
   adl_load (apps/load): one operation on one target, or several at once with `serve`, with no window and no Qt,
   over adl::loader_core and the runner (and adl::loader_scenario, for `--scenario`); a library of its own
   (adl_load_cli) is the program, under a short main
   adl::arinc615a_mini (libs/arinc615a_mini): a second TFTP over the codec, for the tests' simulated
   target and the mock target's engine (libs/mock_target_core)
```

- **`libs/arinc665`** is the ARINC 665 library: loads, batches and media sets
  (written, read and verified), media labels, part numbers, CRCs and check values.
  It is GUI-free and platform-free. It includes no operating
  system header and nothing from Qt.
- **`libs/media_tree`** puts a media set on a disk: the members of a set written to
  a directory tree, one directory per member, and read back, with the standard's
  backslash pathnames mapped to native paths. It uses `std::filesystem`, which is
  the same on every platform, so it holds no platform code and `adl::arinc665`
  keeps its freedom from file I/O. It refuses names that cannot be files on every
  platform, so that a tree written on one system reads on another, and never
  overwrites or follows a link.
- **`libs/loader_core`** is what the loader does that is not showing a window, with no Qt: a media set on
  disk read and checked into a report (`check_media_set`), the index of its files and the `FileSource` an
  upload reads them through, the `DNLD_DATA_<THW_ID_POS>_<number>` folder a download is written into, the
  target's address, and one operation (an `OperationPlan` of what was chosen, `run_operation` over the host's
  sockets with the operator's `OperationGate`) reduced to a result and a reason. `run_operation` holds the
  loader's control port for the one operation, so two of them cannot share port 59 on one address; the
  `OperationPool` (`operation_pool.hpp`) holds it once and runs one operation for each target at once, each
  with its own engine object and transfer sockets, giving every datagram on the control port to the operation
  whose target sent it (one address has one operation at a time, and a datagram from an address with none is
  dropped and counted). It runs on the thread that calls its `step()` and builds its results with the same
  `make_result` as `run_operation`. It also holds the record of
  an operation (`OperationRecord`), the recorder that fills it from what the engine reports
  (`OperationRecorder`, an `OperationListener` that logs in English, in the window's sentences) and the writer
  of the file the record is kept in ([the operation record](file-reference.md#the-operation-record)). The
  loader's application (`apps/app/core`) calls the first part and puts what it returns in the operator's words
  and in Qt's types, so a program with no window can do the same by the same rules; the window does not use
  the recorder. It is in the [development package](libraries.md).
- **`libs/set_maker`** is the media set generator without a window and without Qt, and is not in the
  development package. A `SetPlan` is what a person decides about a media set (the files, the part numbers,
  the members, the check value types, a batch, and more than one load). `validate` holds a plan to the rules
  of the generator's window. `make_set` reads the files, builds the loads and the set, writes the members to a
  folder beside the set's place, reads that back with `adl::loader_core::check_media_set` (the loader's own
  check) and only then moves it into place, removing what it wrote whatever fails; its failures are values
  that the window turns into its words in eight languages and the command into English. `inspect_set` reads
  any media set into the facts that a manifest or a comparison needs. The generator's window
  (`apps/media_gen/core`) and the command `adl_make_set` (`apps/make_set`) both call it, so that they cannot
  differ.
- **`libs/samples`** is the sample loads and media sets (the golden scenario of
  `tests/golden/make_golden.py` and the ARINC 615 and Boeing 777 layouts of
  Appendix F), as data. The tests build from it, and so does
  **`apps/write_payloads`**, the `adl_write_payloads` program that writes them to a
  directory so that other loaders can be tried on them.
- **`apps/mock`** is `adl_mock`, the mock target as a command: no window and no Qt, a function
  (`adl::mock::cli::run_command`) over `libs/mock_target_core` with a command line, one line or one JSON
  object for each event, and exit codes. `main()` only calls the function, so the tests run it in a thread
  and put the loader's real engine against it.
- **`apps/load`** is the `adl_load` program: one ARINC 615A operation on one target, or FIND,
  with no window and no Qt, for scripts and other programs ([the command](load-command.md)). It is
  a static library of its own, `adl_load_cli` (the options, the events as JSON lines or plain lines,
  and the run function that takes a platform host and the streams it writes to), and a `main` that
  makes the real host. The library is not exported and not installed, so that a test can run a
  whole command on the fake platform; the work of an operation is `adl::loader_core`. `adl_load serve`
  is the one command that runs several operations at once: its loop (`serve.cpp`) takes request lines
  from a `LineSource`, the program's standard input read on a thread of its own (`stream_source.cpp`) or a
  test's script, and runs them on the loader core's `OperationPool`, so that one process holds the
  loader's control port for all of them. What an upload reads before it can start, its media set, is done
  by a `Preparer` (`preparer.cpp`) on a thread of its own, so that a large set does not hold up the
  operations that are running. With `--scenario` (`scenario_run.cpp`) the file is read first, through the
  platform's file system, and a `ScenarioRun` gives the engine the player of `adl::loader_scenario` as its
  fault hook and writes each fault it does as a `scenario_step` event; without the option the engine has no
  hook. `adl_load` is built on a desktop only, since the scenario file is read with the licence library's
  reader.
- **`libs/platform`** is the only place that knows which operating system this
  is. Each thing the program needs from the system is a header in
  `libs/platform/interfaces/adl/platform/`, with one implementation per platform
  beside it: files, memory, threads and priorities, a clock, UDP sockets and
  message queues, gathered in a `Host` that the runner is handed (a real one
  from `make_host()`, or the fake in `tests/fake`). [Platform
  isolation](platform-isolation.md) has the rules and
  [platform abstraction](platform-abstraction.md) the design, how each of seven operating systems and two INTEGRITY
  ports (which have [a page of their own](platform-ghs.md)) provides each service, and what has and has not been
  verified.
- **`libs/arinc615a`** is the ARINC 615A library, the data loader's side of the protocol. Its codec
  is the TFTP packets with their options (and the WAIT and ABORT messages), the FIND packets and the
  twelve protocol files of section 6.4, each written into a buffer the caller supplies and read back
  as views over the bytes, with every length checked. Its engine is **sans-I/O**: state machines
  driven by the time and the bytes that arrived, that return the bytes to send and the next time they
  want to be woken, with no thread, socket, clock or file system inside. `TftpTransfer` is one file
  transfer (client or server, sending or receiving); `DataLoader` runs an Information, Upload or
  Download operation over a few of them at once, with the time-outs and retries of Attachment 4 and the
  abort paths; `FindClient` is FIND. The files of an upload and a download are reached through two small
  interfaces, `FileSource` and `FileSink`. It is platform-free and allocates nothing of its own (the one exception
  is the value of a checksum, which `adl::arinc665` returns in a `std::vector`): no `std::string`
  or `std::vector`, no exceptions, no `iostream`, and it never builds an `adl::arinc665` model (it uses
  that library's check value numbers and checksums only). [ARINC 615A codec](arinc615a-codec.md) has the
  layouts, [ARINC 615A engine](arinc615a-engine.md) the design and the choices made where the standard is
  unclear, and [ARINC 615A coverage](arinc615a-coverage.md) the standard section by section.
- **`libs/arinc615a_mini`** is a second, independent TFTP implementation (lock-step blocks, the time-out and
  retry, the options the loader uses, WAIT and ABORT), deliberately plain and written on the codec, so that the engine is checked against another reading of the standard and not against itself. It uses the codec and the figures in `limits.hpp` (the time-outs, the retry count and the default block size), so those are not independently checked. It is the TFTP of the simulated target of `tests/engine` and of the mock target application.
- **`libs/mock_target_core`** is the mock target's logic without a window and without Qt: the engine that
  behaves as an ARINC 615A target (`MockEngine`, event-driven and single-threaded, speaking through the
  `TargetLink` seam) and the link that runs it over the platform's UDP sockets. It is not installed and
  not in the `adl` package. The mock target application runs it on a worker thread, and the tests run it
  against the loader's engine on the fake platform and over loopback sockets.
- **`libs/loader_scenario`** makes the loader misbehave on purpose, repeatably, so that a supplier can see how a
  target reacts: a scenario is an ordered list of steps, each of which says when it is in effect, which of the
  datagrams the loader sends it acts on, and what it does to them (drop one, send it twice or late, change a byte, a
  check value or the length, answer with an ERROR, WAIT or ABORT, abort at a chosen moment, go silent). The `Player`
  carries a scenario out through the engine's `FaultHook` (`libs/arinc615a/include/adl/arinc615a/fault_hook.hpp`),
  which is the only place the engine knows of faults. The top level of the file, the reading of `when` and the
  random numbers are `libs/scenario_base`'s, which it shares with the mock target's scenario file; the loader's own
  are the `on`, `count` and `do` of a step and the player. No Qt, no platform code; it is on a desktop only (the
  reader uses the licence library's strict JSON reader) and is not part of the installed `adl` package.
- **`libs/mock_scenario`** is the mock target's scenario: a file that says which fault happens when (the nth request, the
  nth status file, a time since the operation began; any fault the mock target has, a delay, or a fault on a datagram the target sends: lost, repeated, late or reordered), read over the shared
  reader of `libs/scenario_base`, written back in one form, and a `ScenarioPlayer` that is the `FaultPlan` the engine asks.
  The engine knows nothing of files or seeds: `adl/mock/fault_plan.hpp`, in `libs/mock_target_core`, is the interface, in
  the engine's own terms (wait, refuse, drop a status file, abort, go silent, corrupt, wrong check value, and for each datagram the engine is about to send: send, drop, repeat, delay or hold). A
  desktop library, like the licence library it reads JSON with; not installed. `adl_mock` and the mock target's window
  both take a scenario through it.
- **`libs/arinc615a_runner`** is the thin layer that is the only 615A code that calls the platform: it
  opens the sockets the engine asks for, waits on all of them from one thread with
  `Network::wait_readable`, reads the platform's monotonic clock, and carries out what the engine answers.
  It also has a `FileSource` and a `FileSink` over a directory of the platform's files. That is what
  makes the engine portable to an RTOS (a port replaces the runner) and testable without a network (the
  tests replace the platform with the fake and the target with a simulated one).
- **`libs/licence`** reads and judges a licence file: the signed envelope (Ed25519, by Monocypher in
  `third_party/monocypher`), the payload, the state a licence is in (none, valid, expiring, grace, expired,
  not yet valid, clock suspect or invalid), what it grants, the clock check and the revocation list. It is
  GUI-free and platform-free: it reads no clock and does not ask the computer's name, so the caller passes
  them in, and it reaches files only through a small interface of its own (`Files`, in `store.hpp`) that
  the caller fills in. So every state is tested, a full disk included. On top of that interface is the
  store: the licence files of a user's folder and of a folder an administrator keeps (up to 64 of each, in
  name order, the best one wins), importing a file (never replacing one), removing one from the user's
  folder, the signed revocation lists found beside the licences, and the clock file. It is on a desktop only
  and is not part of the installed `adl` package. The three Qt applications link it, through the licence
  model of `libs/ui` (`adl::ui::LicenceModel`, which words the licence for the windows and keeps the
  Licence dialog, the notice, the edition chip and the Support dialog current); `adl_write_payloads` does
  not. Nothing a program does depends on a licence: the windows show what it says and switch nothing off.
  Signing is a separate target (`adl_licence_sign`) for the owner's tool and the tests, and so is the
  published test key (`adl_licence_test_keys`); no program links either. The library takes no product name except in
  `product.hpp`, `trusted_keys.cpp` and `revocations_release.cpp`, so that it can move into a repository
  that more than one product shares. Its strict JSON reader (no repeated key, no comment, no trailing comma,
  UTF-8 only, eight levels deep) is also public as `adl/licence/json.hpp` (`adl::licence::json`), for the
  other files of this repository that have to mean one thing to every reader, such as a scenario file.
- **`libs/licence_platform`** is the part of the licence work that is this product's: the store's `Files`
  interface filled in with `adl::platform::FileSystem`, and the folders ARINC Data Loader keeps its licences
  and its clock file in (below the platform's user and machine folders, or below `ADL_SETTINGS_DIR`, which a
  program passes in, so that a test never reads a real licence). It is under the GPL alone, where `libs/licence`
  is also offered under the BSD 2-Clause licence ([licensing](licensing.md)), because it uses `adl::platform`.
  It is on a desktop only, is not part of the installed `adl` package, and the three Qt applications link it
  (through the licence model of `libs/ui`).
- **`libs/licence_cli`** is the commands of `adl_licence`: `show`, `import`, `remove` and `verify`, over the store and
  `libs/licence_platform`. `adl::licence_cli::run` takes the command line and an `Environment` (the files, the folders, the
  time, the computer's name and the keys) and writes to two streams, so that a test gives it files in memory, a clock of
  its own and the published test key, and `SystemEnvironment` fills the `Environment` in from this computer. It links
  `adl::licence` and no signing code, so no program that ships holds a key or the test key. It is under the GPL alone, on a
  desktop only, and not part of the installed `adl` package.
- **`apps/licence`** is the `adl_licence` program around those commands: its arguments through `adl::platform::arguments`,
  and the one thing `adl::platform` does not give, an environment variable (`ADL_SETTINGS_DIR`), which is the one part
  that differs between systems: CMake compiles `os/<platform>/environment.cpp` chosen by the platform's name, as it does
  for the entry point of the owner's tool. It can move into `adl::platform` if a second program needs it. The target is
  `adl_licence_program`, because `adl_licence` is the library of the licence; the file is `adl_licence`. A program of the
  tests, `adl_licence_test_program`, runs the same commands with the test key and a clock the test sets, and is never
  installed.
- **`tools/licence`** is the owner's licence tool (`adl_licence_owner`): it makes a signing key, issues a licence file,
  checks one and writes a revocation list, on `adl::licence`, `adl::licence_sign` and the test key and the standard
  library, and nothing else of this repository. It is built with the other tools and never installed; a package check
  fails a release that holds it. Its entry point is the one part that differs between systems, and CMake picks it by
  the platform's name ([the tool](licence-owner-tool.md)).
- **`libs/scenario_base`** is what the scenario files of the mock target and of the loader have in common: the
  random numbers (SplitMix64, with integer arithmetic only, so that a seed gives the same numbers on every system
  and a run with faults can be repeated) and the reading of the top level of a file and of the shape of its steps,
  with the strict JSON reader of the licence library (`adl/licence/json.hpp`). It is a desktop library, like the
  licence library it reads with, and is not installed. The two kinds of scenario are built on it and add their own
  actions.
- **`apps/app`**, **`apps/media_gen`** and **`apps/mock_target`** are the Qt 6.12 applications, thin layers
  over the libraries: a window to choose a media set, run FIND, pick a target and run the Information,
  Upload and Download operations; a generator for the media set those operations load; and a mock
  target over real sockets. The engine runs on a thread of its own (the runner blocks), reports to the
  window through queued signals, and is aborted through an atomic flag the runner polls. Each
  application's `core/` is its logic without a window, tested headless; each `qml/` is its view.
  **`libs/ui`** is the shared design system and window chrome for all three (the same QML source is
  compiled into each application's module). Qt is used in these directories only, and the
  applications build when `ADL_BUILD_APP` is on (the default on a desktop where Qt 6.12 is found). [The application](app.md) has the threading,
  what it shows and what it does not do yet; [the design overhaul](design-overhaul.md) has the
  human-factors analysis behind the three windows.

A library never depends on one above it. `adl::arinc665` does not link
`adl::platform`; only a library that needs the system does.

## The embedded direction

The roadmap includes a build of the loader library for VxWorks 178C. The platform
layer is shaped for it: a platform is a directory and a branch in the selector;
interfaces are small enough for an RTOS with a flash file system or none; memory
comes from caller-supplied buffers, an arena and a block pool; priorities are
abstract; the engine is driven by time and bytes, not by threads; and nothing
throws. What is still not true is "no allocation after start-up": the ARINC 665
model types hold `std::string` and `std::vector`. [Platform
abstraction](platform-abstraction.md#area-2-memory-memoryhpp) says where, and the
route to remove it.

## Targets and names

| Directory | Target | Alias | Namespace | Headers |
| --- | --- | --- | --- | --- |
| `libs/arinc665` | `adl_arinc665` | `adl::arinc665` | `adl::arinc665` | `adl/arinc665/*.hpp` |
| `libs/media_tree` | `adl_media_tree` | `adl::media_tree` | `adl::media_tree` | `adl/media_tree/*.hpp` |
| `libs/loader_core` | `adl_loader_core` | `adl::loader_core` | `adl::loader_core` | `adl/loader_core/*.hpp` |
| `libs/samples` | `adl_samples` | `adl::samples` | `adl::samples` | `adl/samples/*.hpp` |
| `libs/arinc615a` | `adl_arinc615a` | `adl::arinc615a` | `adl::arinc615a` | `adl/arinc615a/*.hpp` |
| `libs/arinc615a_runner` | `adl_arinc615a_runner` | `adl::arinc615a_runner` | `adl::arinc615a` | `adl/arinc615a/runner.hpp`, `directory_files.hpp` |
| `libs/arinc615a_mini` | `adl_arinc615a_mini` | `adl::arinc615a_mini` | `adl::arinc615a_mini` | `adl/arinc615a_mini/*.hpp` |
| `libs/licence_platform` | `adl_licence_platform` | `adl::licence_platform` | `adl::licence_platform` | `adl/licence_platform/*.hpp` |
| `libs/licence_cli` | `adl_licence_cli` | `adl::licence_cli` | `adl::licence_cli` | `adl/licence_cli/*.hpp` |
| `libs/licence` | `adl_licence` (with `adl_licence_sign` and `adl_licence_test_keys`) | `adl::licence` (and `adl::licence_sign`, `adl::licence_test_keys`) | `adl::licence` | `adl/licence/*.hpp` |
| `libs/scenario_base` | `adl_scenario_base` | `adl::scenario_base` | `adl::scenario` | `adl/scenario/*.hpp` |
| `apps/app/core` | `adl_app_core` | `adl::app_core` | `adl::app` | `adl/app/*.hpp` |
| `apps/media_gen/core` | `adl_gen_core` | `adl::gen_core` | `adl::gen` | `adl/gen/*.hpp` |
| `libs/mock_target_core` | `adl_mock_target_core` | `adl::mock_target_core` | `adl::mock` | `adl/mock/*.hpp` |
| `libs/mock_scenario` | `adl_mock_scenario` | `adl::mock_scenario` | `adl::mock` | `adl/mock/scenario*.hpp` |
| `apps/mock` | `adl_mock_cli` | `adl::mock_cli` | `adl::mock::cli` | `adl/mock/cli/*.hpp` |
| `apps/mock_target/core` | `adl_mock_core` | `adl::mock_core` | `adl::mock` | `adl/mock/model.hpp` |
| `libs/platform` | `adl_platform` | `adl::platform` | `adl::platform` | `adl/platform/*.hpp` |
| `libs/ui` | `adl_ui_message` | `adl::ui_message` | `adl::ui` | `adl/ui/message.hpp` |

The programs are `adl_app`, `adl_media_gen`, `adl_mock_target`, `adl_mock`, `adl_write_payloads`, `adl_make_set`, `adl_load` and `adl_licence`. The rest of `libs/ui` is not a library: `cmake/AdlUi.cmake` adds its sources and QML to each of the three Qt
applications.

A public header is included as `"adl/<library>/<name>.hpp"`; the include
directory of a library is its `include/` (or, for the platform, `interfaces/`).
The `adl::warnings`, `adl::sanitizers`, `adl::coverage` and `adl::tracy` targets
are linked into every first-party target by the build, not by the libraries'
users.

## Names in the code

The repository keeps the names it has, and `.clang-tidy` states them (`readability-identifier-naming`, checked
by the nightly clang-tidy run). They are not the organisation template's; where the two differ, this
repository's own convention governs its code.

| Kind | Case | Example |
| --- | --- | --- |
| Namespaces, functions, methods, variables, parameters | `snake_case` | `adl::arinc665::decode_header_file` |
| Classes, structs, enums, template parameters, type aliases | `PascalCase` | `MediaSetIndex`; a standard-library alias keeps its name (`value_type`) |
| Enumerators | `snake_case` | `CheckValueType::md5` |
| Constants, `constexpr` variables | `snake_case` | `sample_load_part_number` |
| Private and protected data members | `snake_case` with a trailing underscore | `current_language_` |

The classes QML reads (`apps/*/core` and `libs/ui`) name their properties, signals, slots and invokable functions
in `camelCase`, which is QML's convention, and the roles of a list model take Qt's `PascalCase` (`SourcePathRole`);
the `.clang-tidy` file in each of those directories allows both cases for a method. A new class follows the case of
its neighbours.

## Command-line programs

A program that is started from a command line (`adl_write_payloads`, `adl_mock`, `adl_make_set`, `adl_licence`, and any other that does not open a
window) is written this way:

- **Its `main` takes its arguments from `adl::platform::arguments` and never reads `argv` itself.**
  `adl/platform/arguments.hpp` has it: `const std::vector<std::string> arguments = adl::platform::arguments(argc, argv);`
  is the first line. On Windows `argv` is the command line narrowed to the system's code page, in which a folder or
  file named with a character outside the page (Japanese on a Western system) has already been lost, and nothing
  tells the program. `arguments` reads the command line of the process instead and returns each argument as UTF-8, on
  every system, with the program name first, as `argv` has it.
- **A path comes from an argument with `adl::platform::path_from_utf8`**, never from `std::filesystem::path{argument}`:
  on Windows that reads the bytes in the code page, and makes another name without an error.
- **Text for the person comes from a path with `to_utf8(path.u8string())`** (`paths.hpp`), never `path.string()` or
  `path.generic_string()`: on Windows those are the code page again, and throw for a character outside it.
- **`apps/make_set`** is `adl_make_set`: the program is `run_command` in a library of its own (`adl_make_set_cli`,
  not installed) so that the tests run it on temporary folders, and `main.cpp` is only the call. It has no Qt, and
  it is a desktop program, because the description file is read with the licence library's strict JSON reader
  (`adl/licence/json.hpp`), which only a desktop build has. What it writes is JSON written by hand
  (`json.hpp`): events, one line each, and the inspection document, one line.
- The three Qt programs take their arguments through Qt, which reads the command line itself, and are not
  concerned.

[Platform abstraction](platform-abstraction.md#the-programs-arguments-argumentshpp) has how each platform does it.

## Choices the libraries make

- **C++23, extensions off.** Every first-party target is built with warnings as
  errors (`cmake/CompilerWarnings.cmake`), and Debug builds run under
  AddressSanitizer (and UBSan where the compiler has it).
- **Failures are values.** An operation that can fail returns
  `std::expected<T, adl::arinc665::Error>` (`adl::platform::Result<T>` in the
  platform layer, whose interface is `noexcept` throughout; `adl::arinc615a::Error` in the 615A library and
  `adl::media_tree::TreeError` in the tree writer). Nothing in the
  library throws for an expected failure. (Running out of memory is not an expected failure: the model
  types hold `std::string` and `std::vector`, so the library is not yet free of
  the heap.)
- **Nothing happens before `main`.** The CRC tables are computed by the compiler
  (`constexpr`) and a checker is chosen by calling `IntegrityChecker::make`. The
  earlier design registered checkers in a factory from static initialisers, which
  depends on the order translation units are initialised in.
- **Closed sets are variants.** The five check-value algorithms (three CRCs, MD5 and SHA-1) are held in a
  `std::variant`, so choosing one at run time costs no allocation.
- **Values, not hierarchies.** The load model is plain types with defaulted
  comparison. There is no virtual `OutputStream`: writing a file is a function
  that returns bytes, not a stream operator on a base class.
- **No `iostream` in `adl::arinc665`.** It takes and returns spans of bytes. Files
  and sockets belong to the layers above: sockets through `adl::platform`, files
  through `adl::media_tree`. A media set is read from `MediaMember`s, the files of
  each in memory; getting them from a directory is `adl::media_tree`'s job.
- **Three layers inside the library.** The bottom is bytes: `WordWriter` and
  `WordReader` (private, `libs/arinc665/src/wire.hpp`), the CRCs, MD5 and SHA-1. Above it each
  file format is a plain model type with an `encode_*` and a `decode_*` function:
  `HeaderFile`, `BatchFile`, `LoadsList`, `FilesList`, `BatchesList`. At the top
  are the operations that need several files, `build_load`/`verify_load` and
  `build_media_set`/`read_media_set`.
- **A decoder reads by pointer and checks what it reads.** Lengths, pointers and
  CRCs are checked before a field is believed, and a damaged file is an
  `Error`, not undefined behaviour: `tests/unit/robustness_test.cpp` runs each
  decoder over every truncation and bit change of a good file.
- **"Must" is an error, "should" is a finding.** Where the standard requires
  something (a data file, a CRC that matches) the operation fails; where it
  recommends (a part number format, a file name) the work goes ahead and a
  `Finding` names the section. A loader has to read loads that are not
  perfect.

## From the earlier project

| Earlier | Now |
| --- | --- |
| `ARINC 665` static library (`ARINC_665::*`) | `adl::arinc665` |
| `ARINC 615A` static library (empty) | `adl::arinc615a` (the codec and the sans-I/O engine) and `adl::arinc615a_runner` |
| `IIntegrityChecker`, `IntegrityChecker_CRC<>`, `IntegrityCheckerFactory`, `Singleton` | `Crc<>`, `Md5`, `Sha1` and `IntegrityChecker::make`; the singleton is gone |
| `LoadCheck` | `CheckValueType` (the type) and `IntegrityChecker` (the calculation) |
| `GenericFile`, `UserSuppliedFile`, `DataFile`, `SupportFile` | `UserSuppliedFile`, `DataFile`, `SupportFile`, `ConfigLdrFile` and `ExcConfigLdrFile`, plain structs in `files.hpp`; `GenericFile` has no counterpart |
| `HeaderFile` | `HeaderFile` (the fields of the header file) with `encode_header_file` and `decode_header_file` |
| `Load` | `LoadSpec` (what a person decides), `Load` (the header, its bytes and the files), `build_load`, `verify_load` |
| `MediaSet`, `MediaSetMember` | `MediaSetSpec`, `MediaMember`, `MediaSet`, `build_media_set`, `read_media_set`; `LoadsList`, `FilesList`, `BatchesList` |
| C++/CLI MSTest project | Catch2, in `tests/unit` |

The earlier code (which is not in this repository) did not do most of this: its header file `OutputStream` did not
compile and its pointers were placeholders, writing a load to a directory
returned `false`, and MD5 and SHA-1 were declared and never written. Writing a
media set to a directory and reading one from it is `adl::media_tree`.

## The standards

Section numbers in comments
and tests refer to ARINC 665-3 and ARINC 615A-3, which are not in the repository
([docs/specs](specs/README.md)). [ARINC 665 coverage](arinc665-coverage.md) maps
the standard onto the code.
