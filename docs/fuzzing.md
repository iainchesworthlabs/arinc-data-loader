# Fuzzing

*For developers. What CI runs is [CI](ci.md); the tests that hold the decoders down by hand are in [Testing](testing.md).*

Everything that reads bytes from outside has a [libFuzzer](https://llvm.org/docs/LibFuzzer.html)
harness: the files of an ARINC 665 media set, the packets and protocol files of ARINC 615A, a licence file, a scenario file,
the text of a target's address, the description file of `adl_make_set` and the members of a media set that it inspects, and the command line of a program (the platform layer's split of it and
its text conversions). A
decoder returns an error for a bad input and never crashes; the fuzzers are there to find the input for
which that is not true.

| Directory | Targets (`adl_fuzz_<name>`) |
| --- | --- |
| `libs/arinc665/fuzz` | `header_file` (a load's `.LUH`), `batch_file` (`.LUB`), `loads_list` (`LOADS.LUM`), `files_list` (`FILES.LUM`), `batches_list` (`BATCHES.LUM`). A decoded file is also checked against the standard and written back. |
| `libs/loader_core/fuzz` | `ipv4_address` (the text of a target's address, as an operator types it; a text that parses is written out and parsed again and has to give the same four numbers). |
| `libs/set_maker/fuzz` | `description` (the bytes of a description file for `adl_make_set`: a text that is refused says why in a sentence; one that is accepted is inside every limit of format 1, passes the generator's rules, and is written out as a canonical description that reads back as the same one) and `inspect_set` (the files of a media set, as a list of files in a small container that `libs/set_maker/fuzz/checks.hpp` describes: what does not read as a media set is refused with a reason, and what does is inspected and written as the document of `adl_make_set inspect --json`, which the strict JSON reader has to read). A third, `options`, is the command line of `adl_make_set` (one argument to a line): what cannot be used is refused with a sentence, and what can is a command with its subject and only the options that belong to it. The harnesses are built from `apps/make_set`, where the reader is. Their checks are functions of `checks.hpp`, which the test `make_set.fuzz` also runs on every seed and every prefix of one, and on the disk path of `inspect` (each set seed is written out as a tree and read back), so a build with no Clang knows the seeds are good. The seeds are written by `tools/fuzz/make_seeds.py`: the guide's example description and a refusal of each kind, and the golden media set of `tests/golden/make_golden.py` whole and in four ways of being wrong, and command lines that are right and wrong. |
| `libs/platform/fuzz` | `command_line` (the split of a Windows command line and the conversions between UTF-8 and UTF-16 in `libs/platform/detail/command_line.hpp`; the harness checks that what comes out is UTF-8, which converts to UTF-16 and back unchanged). Its seeds are command lines, as UTF-16 with the low byte first. |
| `libs/arinc615a/fuzz` | `tftp_packet` (the datagrams of the TFTP dialogue, with the transfer options of a request or an option acknowledgement), `find` (a FIND packet), and one for each protocol file: `initialization_file`, `configuration_list`, `configuration_status`, `upload_request`, `upload_status`, `download_request`, `download_status`, `download_list`, `download_answer`. |
| `tools/licence/fuzz` | `owner`: what the owner's tool reads that is not a licence file ([the tool](licence-owner-tool.md)): a public key file (what is accepted is what the writer writes, and reads back as the same key), the words of a command line, a UTF-16 command line, a licence number and a key id. The seeds are written by `tools/fuzz/make_seeds.py`, as they have no golden data. |
| `libs/mock_target_core/fuzz` | `parse_options` (the command line of `adl_mock`, which is not bytes but comes from outside all the same: the input is the arguments with a line feed after each one, so its seeds are text; what is accepted is checked against the limits the usage text states, and an address that is read is written back as it was). The harness is kept here because the workflows look in `libs/*/fuzz`; the program it fuzzes is `apps/mock`. |
| `libs/mock_scenario/fuzz` | `scenario`: the input is the mock target's scenario file. A file that is refused says why in one line; one that is read is checked against the limits it states (an action goes with its trigger, a step that draws has a seed, every number in its range, `on` and `count` only with an action on datagrams), written and read again as the same scenario, and played for a few moments. The seeds are written by `tools/fuzz/make_seeds.py`, and the `mock.scenario` test replays them under the ordinary compilers too. |
| `libs/licence/fuzz` | `licence`: the signed envelope, the strict JSON reader, the licence payload, a revocation list, the clock file the store keeps and the dates, all from one input. What a reader accepts is written back and read again, and has to come out the same. The seeds are licence files and payloads signed with the published test key, and two clock files. |
| `libs/scenario_base/fuzz` | `envelope`: the input is a scenario file, read as the mock target's and as the loader's. It checks that a file that is refused says why in one line, and that one that is read keeps every number inside its limit, the steps numbered from 1, a seed for every step that draws, and nothing but the keys the kind named. The seeds are written by `tools/fuzz/make_seeds.py`, as they have no golden data, and the `scenario.base` test replays them under the ordinary compilers too. |
| `libs/licence_cli/fuzz` | `licence_cli`: the commands of `adl_licence` ([the command](user-guide/command-line.md#adl_licence)): a command line, one word to a tab, and the bytes of a file, which are put where each command can meet them (the file the line names, a licence file and a list of withdrawn licences in each folder, the clock file, and signed with the test key as a licence and as a list). Every run must end with the exit code 0, 1 or 2; `show` and `verify` must write nothing; nothing printed may hold a control character. The seeds are written by `tools/fuzz/make_seeds.py`: one for each command, from the golden licence files. |

A harness is in `libs/<lib>/fuzz` or, for a program of the repository's tools, in `tools/<tool>/fuzz`. Each target has a directory of **seed inputs** (`corpus/<name>/`), written by
`python3 tools/fuzz/make_seeds.py` from the golden data of the tests (`tests/golden`; those of `parse_options` are written by hand: they are command lines, and the `mock.cli` test replays them under the ordinary compilers too), and a directory of
**regressions** (`regressions/<name>/`): inputs that once crashed it.

## Running them

The harnesses are built with Clang, libFuzzer and AddressSanitizer and UBSan, by the `linux-fuzz`
preset (it builds the libraries and the harnesses only, without Qt or the tests):

```sh
CC=clang CXX=clang++ cmake --preset linux-fuzz
cmake --build --preset linux-fuzz
ctest --preset linux-fuzz        # replays the seeds and the regressions once: seconds
```

To fuzz one target, give libFuzzer a directory for what it finds and the committed directories to start
from:

```sh
mkdir -p /tmp/work
build/linux-fuzz/bin/adl_fuzz_tftp_packet -max_total_time=300 /tmp/work \
    libs/arinc615a/fuzz/corpus/tftp_packet libs/arinc615a/fuzz/regressions/tftp_packet
```

A crash leaves the input in a file named `crash-<hash>`. Run the target on that file alone to see
the report again. The sanitizers abort on the first undefined behaviour (`-fno-sanitize-recover=all`).

## When a fuzzer finds something

Fix the decoder, and add the input that crashed it to `regressions/<name>/` beside its harness (`libs/<lib>/fuzz` or `tools/<tool>/fuzz`) in the same
pull request, so that the pull request gate replays it from then on. Give the file a name that says what
it was (`truncated-option.bin`). Do not add the files libFuzzer found that did not crash; the seed
corpus is meant to stay small and readable.

## In CI

- `Fuzz regression (Linux, Clang)` in `ci.yml` builds the `linux-fuzz` preset and runs the replay on
  every ready pull request and every queue entry. It is part of `CI Status`.
- `fuzz.yml` fuzzes each target for 60 seconds on every push to `develop` (one run at a time, with at
  most one waiting), and for ten minutes each night. The inputs it finds are kept between runs in the
  Actions cache; they are not committed. The nightly run is split into three **shards**, each a job of its
  own that builds the harnesses and fuzzes every third of them in name order, ten minutes each, so that it takes
  about two hours and a half (27 targets: nine to a shard, 90 minutes of fuzzing and the build) and not five;
  a shard's limit is 150 minutes, which holds 11 targets, and a shard is added with one number in the list
  `strategy.matrix.shard` of `fuzz.yml` (a hosted job may run for 360 minutes at most). A push to `develop`
  and a manual run use one shard, and a manual run can ask for three. A shard keeps the inputs of its own targets
  in the Actions cache under a key of its own. A crash fails its shard and uploads the input as the
  `fuzz-crashes-shard-<n>` artifact. The last job of the workflow, named `Fuzz (Linux, Clang)` as the one job was,
  ends with the result of all the shards, and on the nightly run opens or updates one `nightly-analysis` issue
  (once, and not by each shard); a shard that hits its limit is cancelled, which counts as a failure there.

- `clusterfuzzlite.yml` runs [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/) on a ready pull
  request that changes `libs/`, `tools/licence/`, `cmake/` or the files in `.clusterfuzzlite/`: it builds the harnesses with
  AddressSanitizer and fuzzes them for ten minutes between them. It is not a required check. The build is
  `.clusterfuzzlite/build.sh` in the image of `.clusterfuzzlite/Dockerfile` (the OSS-Fuzz base builder,
  pinned by digest and moved by Dependabot; its Clang is older than the one the `linux-fuzz` preset uses). The script
  zips the seeds and regressions of each harness from `libs/*/fuzz` and `tools/*/fuzz`, and `fuzz.yml` starts from the
  same directories, so a harness in either place is built, seeded and fuzzed.
  ClusterFuzzLite chooses the sanitizer and passes it in `CFLAGS`, so the build sets
  `ADL_FUZZING_SANITIZERS` empty. To try it before a push, build the image from a clean copy of the
  tracked files and run `compile` in it (`build/` would otherwise be sent to Docker too):

  ```sh
  docker build -t adl-cflite -f .clusterfuzzlite/Dockerfile .
  docker run --rm -e SANITIZER=address -e FUZZING_ENGINE=libfuzzer -e ARCHITECTURE=x86_64 \
      -e FUZZING_LANGUAGE=c++ -v "$PWD/out:/out" -v "$PWD/work:/work" adl-cflite compile
  ```

Fuzzing is separate from the sanitizer and Tracy builds: libFuzzer brings its own AddressSanitizer
runtime, so `ADL_ENABLE_FUZZING` cannot be combined with `ICL_SANITIZERS`, Tracy or coverage.
