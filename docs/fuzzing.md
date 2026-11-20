# Fuzzing

*For developers. What CI runs is [CI](ci.md); the tests that hold the decoders down by hand are in [Testing](testing.md).*

Everything that reads bytes from outside has a [libFuzzer](https://llvm.org/docs/LibFuzzer.html)
harness: the files of an ARINC 665 media set, the packets and protocol files of ARINC 615A, a licence file,
the text of a target's address, and the command line of a program (the platform layer's split of it and
its text conversions). A
decoder returns an error for a bad input and never crashes; the fuzzers are there to find the input for
which that is not true.

| Directory | Targets (`adl_fuzz_<name>`) |
| --- | --- |
| `libs/arinc665/fuzz` | `header_file` (a load's `.LUH`), `batch_file` (`.LUB`), `loads_list` (`LOADS.LUM`), `files_list` (`FILES.LUM`), `batches_list` (`BATCHES.LUM`). A decoded file is also checked against the standard and written back. |
| `libs/loader_core/fuzz` | `ipv4_address` (the text of a target's address, as an operator types it; a text that parses is written out and parsed again and has to give the same four numbers). |
| `libs/platform/fuzz` | `command_line` (the split of a Windows command line and the conversions between UTF-8 and UTF-16 in `libs/platform/detail/command_line.hpp`; the harness checks that what comes out is UTF-8, which converts to UTF-16 and back unchanged). Its seeds are command lines, as UTF-16 with the low byte first. |
| `libs/arinc615a/fuzz` | `tftp_packet` (the datagrams of the TFTP dialogue, with the transfer options of a request or an option acknowledgement), `find` (a FIND packet), and one for each protocol file: `initialization_file`, `configuration_list`, `configuration_status`, `upload_request`, `upload_status`, `download_request`, `download_status`, `download_list`, `download_answer`. |
| `tools/licence/fuzz` | `owner`: what the owner's tool reads that is not a licence file ([the tool](licence-owner-tool.md)): a public key file (what is accepted is what the writer writes, and reads back as the same key), the words of a command line, a UTF-16 command line, a licence number and a key id. The seeds are written by `tools/fuzz/make_seeds.py`, as they have no golden data. |
| `libs/licence/fuzz` | `licence`: the signed envelope, the strict JSON reader, the licence payload, a revocation list, the clock file the store keeps and the dates, all from one input. What a reader accepts is written back and read again, and has to come out the same. The seeds are licence files and payloads signed with the published test key, and two clock files. |
| `libs/licence_cli/fuzz` | `licence_cli`: the commands of `adl_licence` ([the command](user-guide/command-line.md#adl_licence)): a command line, one word to a tab, and the bytes of a file, which are put where each command can meet them (the file the line names, a licence file and a list of withdrawn licences in each folder, the clock file, and signed with the test key as a licence and as a list). Every run must end with the exit code 0, 1 or 2; `show` and `verify` must write nothing; nothing printed may hold a control character. The seeds are written by `tools/fuzz/make_seeds.py`: one for each command, from the golden licence files. |

A harness is in `libs/<lib>/fuzz` or, for a program of the repository's tools, in `tools/<tool>/fuzz`. Each target has a directory of **seed inputs** (`corpus/<name>/`), written by
`python3 tools/fuzz/make_seeds.py` from the golden data of the tests (`tests/golden`), and a directory of
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
  Actions cache; they are not committed. A crash fails the run, uploads the input as the `fuzz-crashes`
  artifact, and on the nightly run opens or updates one `nightly-analysis` issue. The nightly run takes about
  three and a half hours (every target in turn, ten minutes each); the job's limit is 270 minutes, and a run that
  hits it is reported in the same issue.

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
