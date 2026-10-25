# Fuzzing

*For developers. What CI runs is [CI](ci.md); the tests that hold the decoders down by hand are in [Testing](testing.md).*

Everything that reads bytes from outside has a [libFuzzer](https://llvm.org/docs/LibFuzzer.html)
harness: the files of an ARINC 665 media set, and the packets and protocol files of ARINC 615A. A
decoder returns an error for a bad input and never crashes; the fuzzers are there to find the input for
which that is not true.

| Library | Targets (`adl_fuzz_<name>`) |
| --- | --- |
| `libs/arinc665/fuzz` | `header_file` (a load's `.LUH`), `batch_file` (`.LUB`), `loads_list` (`LOADS.LUM`), `files_list` (`FILES.LUM`), `batches_list` (`BATCHES.LUM`). A decoded file is also checked against the standard and written back. |
| `libs/arinc615a/fuzz` | `tftp_packet` (the datagrams of the TFTP dialogue, with the transfer options of a request or an option acknowledgement), `find` (a FIND packet), and one for each protocol file: `initialization_file`, `configuration_list`, `configuration_status`, `upload_request`, `upload_status`, `download_request`, `download_status`, `download_list`, `download_answer`. |

Each target has a directory of **seed inputs** (`corpus/<name>/`), written by
`python3 tools/fuzz/make_seeds.py` from the golden data of the tests, and a directory of
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

Fix the decoder, and add the input that crashed it to `libs/<lib>/fuzz/regressions/<name>/` in the same
pull request, so that the pull request gate replays it from then on. Give the file a name that says what
it was (`truncated-option.bin`). Do not add the files libFuzzer found that did not crash; the seed
corpus is meant to stay small and readable.

## In CI

- `Fuzz regression (Linux, Clang)` in `ci.yml` builds the `linux-fuzz` preset and runs the replay on
  every ready pull request and every queue entry. It is part of `CI Status`.
- `fuzz.yml` fuzzes each target for 60 seconds on every push to `develop` (one run at a time, with at
  most one waiting), and for ten minutes each night. The inputs it finds are kept between runs in the
  Actions cache; they are not committed. A crash fails the run, uploads the input as the `fuzz-crashes`
  artifact, and on the nightly run opens or updates one `nightly-analysis` issue.

- `clusterfuzzlite.yml` runs [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/) on a ready pull
  request that changes `libs/`, `cmake/` or the files in `.clusterfuzzlite/`: it builds the harnesses with
  AddressSanitizer and fuzzes them for ten minutes between them. It is not a required check. The build is
  `.clusterfuzzlite/build.sh` in the image of `.clusterfuzzlite/Dockerfile` (the OSS-Fuzz base builder,
  pinned by digest and moved by Dependabot; its Clang is older than the one the `linux-fuzz` preset uses).
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
