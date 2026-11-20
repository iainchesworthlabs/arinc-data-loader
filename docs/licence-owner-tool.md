# The owner's licence tool

*For the owner of the project and for developers. It is not part of what users install.*

`adl_licence_owner` is the program the owner uses to make a signing key, issue a licence file, check one and write a
revocation list. A licence is a signed file that says what a customer bought; no program is gated by it and nothing is
enforced by the code ([Licensing](licensing.md) has the third-party inventory, and the licence library is in
[Architecture](architecture.md)). The tool is the other half of `libs/licence`: that library reads and judges a file,
this one writes it.

The tool is public, as the library is, because what protects a licence is the private key and not the code. A file it
makes is accepted by a program only if it is signed with a key whose public half is in `libs/licence/src/trusted_keys.cpp`,
and the private half of that key is in no repository. A copy of the tool signs with whatever key it is given, and a
program refuses the result.

- Built with the other tools, and **never installed, packed or exported**: no install rule names it. The release
  fails if a package holds it ([the package check](#the-package-check)).
- Its files are under `GPL-3.0-or-later OR BSD-2-Clause`, as the library's are. It includes nothing of this repository
  but `adl::licence`, and takes its arguments and files through the standard library.
- On a desktop only (Windows, Linux, macOS), as the library is.

## Building and running it

```sh
cmake --build build/<preset> --target adl_licence_owner
build/<preset>/bin/adl_licence_owner --help
```

The program is `adl_licence_owner` (`adl_licence_owner.exe` on Windows). Its commands put what they say on the standard
output, and what went wrong on the standard error as `adl_licence_owner: <sentence>`.

## The commands

```text
usage: adl_licence_owner <command> [options]

The owner's tool for ARINC Data Loader licence files. It is built with the tools and is never installed or shipped.

commands:
  inspect <file>   show what a licence file or a revocation list says
  verify <file>    judge a licence file as a program would, or check a revocation list
  issue            sign a licence request into a licence file
  revocations      sign a revocation list request into a revocation list file
  keygen           make a key pair for signing

options:
  inspect <file> [--test] [--trust <file>]...
  verify <file> [--test] [--trust <file>]... [--now <time>] [--computer <name>]
                [--latest-seen <time>] [--build-time <time>] [--revocations <file>]
  issue --request <file> --out <file> (--test | --key-id <id> --key-file <file>) [--dry-run]
  revocations --request <file> --out <file> (--test | --key-id <id> --key-file <file>) [--dry-run]
  keygen --key-id <id> --purpose licence|revocations --out <folder> [--seed-file <file>]

--test signs and checks with the throwaway key test-rfc8032-1, which no program accepts.
Times are written like 2026-11-13T02:10:00Z. File names and output are UTF-8.
Exit codes: 0 done, 1 could not be done, 2 the command line is wrong.
```

Every output is `Name: value` lines, one fact on each, in the order below, so that a script can read it. The names are
part of the tool's interface.

### inspect

Shows what a file says and how its signature stands, whether or not the signature verifies. It exits 0 when the file
could be read, so it is for looking; `verify` is for deciding.

| Line | Value |
| --- | --- |
| `Format` | The envelope format, 1 |
| `Purpose` | `licence` or `revocations` |
| `Key` | The id of the key that signed it |
| `Test key` | `yes` when the key id begins with `test-` (the line is left out otherwise) |
| `Signature` | `verifies`, `does not verify: <why>`, or `not checked: no key with this id and purpose is known` |
| `Key trusted by this build` | `yes` or `no`, when the signature verifies: whether the key is in `trusted_keys.cpp` |

A licence file then has `Licence` (the licence number), `Product`, `Kind`, `Edition`, `Licensee`, `Contact` and `Email`
(when there are any), `Issued`, `Starts`, `Expires`, `Grace days`, `Seats` (`0 (site licence)` for no seat count),
`Features`, `Computers` (`any` when it names none), and `Updates until`, `Replaces` and `Clock reset` when it has them.
A revocation list has `Sequence`, `Issued`, `Licences withdrawn` and `Keys withdrawn`, each with a `Withdrawn licence` or
`Withdrawn key` line for every entry.

The keys it checks against are this build's own list (empty until the owner adds ADL's public keys), the test key with
`--test`, and each public key file given with `--trust`.

### verify

Judges a licence file as a program would, with the library's `evaluate`, and prints the answer. A revocation list is
checked and read instead. It exits 0 when the licence is in force (`valid`, `expiring` or `grace`) or the list
verifies, and 1 otherwise.

| Option | Meaning |
| --- | --- |
| `--now <time>` | The time to judge at. The default is the clock. |
| `--computer <name>` | The computer's name, for a licence that names computers. The default is none. |
| `--latest-seen <time>` | The latest time the program has seen, for the clock check. |
| `--build-time <time>` | When the program was built, for the clock check. |
| `--revocations <file>` | A signed revocation list (`.adlrev`) that applies. |
| `--test`, `--trust <file>` | The keys, as for `inspect`. |

The output is `Purpose`, then `Key`, `Test key`, `Signature` and `Key trusted by this build` when the signature
verified, then `State` (`none`, `valid`, `expiring`, `grace`, `expired`, `not_yet_valid`, `clock_suspect` or `invalid`),
`Reason` (`ok`, `expired`, `not_yet_valid`, `clock_suspect`, `malformed`, `newer_version`, `bad_signature`, `revoked`,
`wrong_computer`, `wrong_product`), `Why` (a clause, when the reason is not `ok`), `Days left` (while in force) and
`Edition` (`community` when the licence is not in force), and the licence's own lines.

### issue

```sh
adl_licence_owner issue --request requests/ADL-2026-000123.json --out issued/ADL-2026-000123.adllic \
    --key-id lic-2026-a --key-file <the private key file>
```

The **request** is the payload of the licence, in the form the library reads (`adl::licence::parse_payload`): a JSON
object with these fields. A request is the file the owner writes; the tool never makes one up.

| Field | Meaning |
| --- | --- |
| `product` | `adl`. (With `--test` any product token is allowed, so that a licence for another product can be made for a test.) |
| `licence_id` | `ADL-<year>-<six digits>`, for example `ADL-2026-000123`. Six digits that begin with 9 (900000 to 999999) are kept for test licences, and are refused without `--test`. |
| `kind` | `trial`, `annual` or `enterprise` |
| `edition` | `pro` or `enterprise` |
| `licensee` | An object: `name`, and optionally `contact` and `email` (personal data: it is written into the licence and nowhere else) |
| `issued_at`, `starts_at`, `expires_at` | Times, `2026-11-13T02:10:00Z` |
| `grace_days` | Days of grace after the end, 0 to 366 |
| `seats` | 0 for a site licence, or the number of seats |
| `features` | Names, such as `support` and `data_packs`; a name that ends in `.*` covers everything below it |
| `machines` | Computer names, where a trailing `*` matches a prefix; empty means any computer |
| `updates_until`, `replaces`, `clock_reset` | Optional: the version window, the licence this one renews, a token that resets the stored clock once |
| `critical` | `[]`. A name in it that this version does not know makes the library refuse the file as made for a newer version |

`issue` reads the request with the library, writes the payload with `write_payload` (so what is signed is the library's
own form of it, and a request the library would not read back is refused), signs it, writes the file, and then **reads
the file it wrote back through the library**: the envelope, the signature against the public half of the key it was
signed with, and the same payload. If any of that fails, the file is removed and the exit code is 1. The tool never
writes over a file that exists.

`--dry-run` checks the request and prints what the licence will say; it signs nothing, writes nothing and needs no key.
The output of a signed licence is `Wrote`, `Bytes`, `Key`, `Test key`, `Signature` and `Key trusted by this build`, and
the licence's lines.

### revocations

The same as `issue`, for a revocation list. The request is the payload of a list: `sequence` (1 or more; the list with
the highest sequence number is the one a program keeps), `issued_at`, `licences` (the numbers withdrawn), `keys` (objects
with a `key_id` and an optional `from`: files signed at or after that time are refused) and `critical`. The file is
written with the extension `.adlrev`.

### keygen

```sh
adl_licence_owner keygen --key-id lic-2026-a --purpose licence --out <a folder on the drive that will keep the key>
adl_licence_owner keygen --key-id lic-2026-a --purpose licence --out <the same folder> --seed-file <32 bytes you made>
```

Makes a key pair for one purpose, `licence` or `revocations`, and writes two files into the folder. The folder has to
exist, and it may not be inside a git work tree (any folder above it holding `.git`): a key made there could be committed
by mistake. A key id is lower-case letters, digits and hyphens; one that begins with `test-` is for `--test` only.

| File | What it holds | What to do with it |
| --- | --- | --- |
| `<key id>.adlownerkey` | The private key: 43 characters of base64url and a line end. Where the system has file permissions it is readable by its owner only, from before the key is written. | Keep it **offline**, with two encrypted copies on separate media. Never put it in a repository, an email or a chat. Whoever holds it can make licences that programs accept. It is never printed and never logged. |
| `<key id>.adlownerpub` | The public key: one line of JSON, `{"key_id":"lic-2026-a","purpose":"licence","public_key":"<43 characters>"}`. Not secret. | Keep it with the licence records. `--trust` reads it. Add its key to `libs/licence/src/trusted_keys.cpp` and build a release, and the programs of that release accept the licences it signs. |

The extensions are not `.adlkey` and `.adlpub`: the Pro tool that writes a customer's own site keys uses those, in
another format, and two formats under one extension, one of them the owner's licence key, is a mistake waiting to happen.

Make a standby key at the same time, with another id, and put its public half in the same list. If the signing key
leaks, a later release can withdraw it, and the standby is already trusted. A key that signs licences does not sign
revocation lists: make one for each purpose.

#### Where the 32 bytes of a key come from

A key is 32 bytes (an Ed25519 seed, RFC 8032), and there are two ways to give `keygen` them.

**Without `--seed-file`** the tool draws them from `std::random_device`, because the rules of this repository leave it
no operating system call of its own. What that draws from depends on the standard library the tool was built with. This
is from the library sources, read on 2026-11-20 (Microsoft's STL `stl/src/xrngdev.cpp` and the documentation of
`rand_s`, GCC 14's `libstdc++-v3/src/c++11/random.cc`, LLVM's `libcxx/src/random.cpp` and `libcxx/include/__config`):

| Built with | `std::random_device` draws from | If it cannot |
| --- | --- | --- |
| MSVC (Windows) | `rand_s` of the C runtime, which "uses the operating system to generate cryptographically secure random numbers" (it depends on `RtlGenRandom`) | It throws `std::out_of_range`. |
| libstdc++ (GCC, and Clang with libstdc++, on Linux) | The first of these that works: the processor's `RDSEED` instruction, where the processor is an Intel or an AMD one that has it (with `RDRAND` to finish what `RDSEED` lacks), then `RDRAND`, then the kernel's `getentropy`, then `/dev/urandom`. | It throws `std::system_error` or `std::runtime_error` ("device not available"). |
| libc++ (AppleClang on macOS) | `arc4random()`, the system's generator on Apple platforms (the file name `/dev/urandom` in the library's source is only a token there) | It has no failure of its own to report. |

None of the three falls back to a fixed sequence: a failure is an exception, which `keygen` turns into "this computer
gives no random numbers: no key was made". (A libstdc++ built for a target with none of those sources does use a fixed
generator, with seed 5489. The Linux and macOS systems this repository is built on are not that: each has `getentropy`, `/dev/urandom` or `arc4random`.) The
tool also refuses a result that is all zeros or is the test key, which would mean the source is not working. It does not
ask `entropy()`: libc++ answers 0 for some sources that are good, so no library's answer means the same.

**With `--seed-file <file>`** the key is the 32 bytes of that file, exactly, made with a generator the owner chose. The
file is read once and its content is never printed, and the tool leaves the file as it is. A file of any other size
(a key written as text, say) is refused, and so are 32 bytes that are all zero or are the test key. The private key file
holds the same key, so the seed file is to be kept as carefully, or destroyed.

**For the real keys, use `--seed-file`**, with bytes the owner drew on the offline computer that will keep the key,
from the operating system's generator or from a hardware one:

```sh
head -c 32 /dev/urandom > seed.bin                  # Linux and macOS
openssl rand -out seed.bin 32                       # anywhere with OpenSSL
```

```powershell
$seed = [byte[]]::new(32)
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($seed)     # Windows PowerShell and PowerShell 7
[IO.File]::WriteAllBytes('seed.bin', $seed)
```

The reason is that the owner then names the source and can check it, it is the same on every system, and no choice of
standard library stands between the generator and the key (on Linux with an Intel or AMD processor, GCC's library asks
the processor's own instructions before the kernel). The default is sound where the three tables above hold, which is
why it is there, and it is what the tests of the tool use.

## Making the owner's keys

Done once, by the owner, on the offline computer that will keep the keys. The licence key signs licence files and the
revocation key signs revocation lists ([a key signs one purpose only](#keygen)); each has a standby, made now, so that a
leak of one can be answered at once. That is four keys: `lic-2026-a`, `lic-2026-b`, `rev-2026-a` and `rev-2026-b`.

**1. Build the tool** in a checkout of the repository, on the computer that will keep the keys, or build it anywhere
and carry the program. The tool is a program of the build tree and is never installed:

```powershell
cmake --preset windows-msvc-release -DADL_BUILD_APP=OFF -DADL_BUILD_TESTS=OFF
cmake --build --preset windows-msvc-release --target adl_licence_owner
```

The two options leave out Qt and the tests, so neither Qt nor vcpkg is needed ([Building](building.md)). The program is
`build\windows-msvc-release\bin\adl_licence_owner.exe`. On Linux and macOS use the preset `linux-release` or
`macos-release`, and the program is `build/<preset>/bin/adl_licence_owner`.

**2. Choose the folder that will keep the keys**, on the encrypted drive, outside every git work tree (no folder above
it holds a `.git`, so not a checkout of this repository or of any other). `keygen` refuses a folder inside a work tree
and says so. Make the folder first; `keygen` does not:

```powershell
$keys = 'F:\adl-keys'          # the drive that will keep the keys; change it to yours
New-Item -ItemType Directory -Path $keys
$tool = '.\build\windows-msvc-release\bin\adl_licence_owner.exe'
```

**3. Make the licence key**, from a seed file of 32 bytes the system's generator draws now
([why a seed file](#where-the-32-bytes-of-a-key-come-from)):

```powershell
$seed = [byte[]]::new(32)
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($seed)
[IO.File]::WriteAllBytes("$keys\lic-2026-a.seed", $seed)
& $tool keygen --key-id lic-2026-a --purpose licence --out $keys --seed-file "$keys\lic-2026-a.seed"
```

**4. Make the revocation key**, the same way with its own seed and its own id and purpose:

```powershell
$seed = [byte[]]::new(32)
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($seed)
[IO.File]::WriteAllBytes("$keys\rev-2026-a.seed", $seed)
& $tool keygen --key-id rev-2026-a --purpose revocations --out $keys --seed-file "$keys\rev-2026-a.seed"
```

**5. Make the two standbys**, the same two commands with the ids `lic-2026-b` and `rev-2026-b` (and the seed files
`lic-2026-b.seed` and `rev-2026-b.seed`).

On Linux and macOS each of steps 3 to 5 is the same, with the folder in `keys=/media/adl-keys` (any folder outside a
work tree) and `tool=build/linux-release/bin/adl_licence_owner`:

```sh
head -c 32 /dev/urandom > "$keys/lic-2026-a.seed"
"$tool" keygen --key-id lic-2026-a --purpose licence --out "$keys" --seed-file "$keys/lic-2026-a.seed"
head -c 32 /dev/urandom > "$keys/rev-2026-a.seed"
"$tool" keygen --key-id rev-2026-a --purpose revocations --out "$keys" --seed-file "$keys/rev-2026-a.seed"
```

Each `keygen` prints `Private key:` and `Public key file:` (the two files it wrote), `Key:`, `Purpose:`, `Key source:`
(the seed file, read and not changed), `Public key:` (43 characters of base64url) and `Public key (hex):` (64
characters). It does not print the private key. It has read both files back before it says anything, and if it could
not, it removed them and exited 1. The folder now holds, for each key, a `.adlownerkey`, an `.adlownerpub` and the
`.seed` it was made from.

**6. Destroy the seed files**, which are the keys again (the `.adlownerkey` holds the same 32 bytes), or keep them
exactly as carefully as the `.adlownerkey` files:

```powershell
Remove-Item "$keys\*.seed"
```

**7. Keep the private halves.** Copy the four `*.adlownerkey` files onto **two encrypted copies on separate media** and
keep the media apart. Never put one in a repository, an email or a chat; the release's package check fails a package
that holds a file with that name. The four `*.adlownerpub` files are not secret: keep them with the licence records.

**8. Give the public halves to the programs.** Add one entry for each of the four to `builtin_keys()` in
`libs/licence/src/trusted_keys.cpp`: the key id (`lic-2026-a`), the purpose (`licence` or `revocations`) and the 32
bytes of the public key (the `Public key (hex)` line, two characters to a byte, or the `.adlownerpub` file), in a pull
request, and release. A program of that release accepts a licence signed with `lic-2026-a` and a revocation list signed
with `rev-2026-a`; a program of an earlier release does not know these keys and does not accept their files.

**9. Check it** with the first licence you issue ([`issue`](#issue) has the request), signed with the private key of
`lic-2026-a`, and `verify` with its public key file:

```powershell
& $tool issue --request .\request.json --out .\licence.adllic --key-id lic-2026-a --key-file "$keys\lic-2026-a.adlownerkey"
& $tool verify .\licence.adllic --trust "$keys\lic-2026-a.adlownerpub"
```

`Signature: verifies` says the file was signed by the key of that public file. `Key trusted by this build: yes` says the
tool itself was built from a source that holds that key in `builtin_keys()`, as a program of that release does; with a
tool built before step 8 it says `no`, and without `--trust` that tool says `State: invalid` and `Reason: newer_version`
(the library reads a key it does not know as one of a newer release), which is also what a program built before step 8
says of the file.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | The command did what it was asked. For `verify`: the licence is in force, or the list verifies. |
| 1 | The command could not be done: a file could not be read or written, a request or a key was refused, the signature does not verify, the licence is not in force, or the file written could not be read back. Nothing is left behind that is not a good file. |
| 2 | The command line is wrong: no command or an unknown one, an option the command does not take, a value missing, an option given twice, a time or key id in a form the tool does not read. |

## What it says

Every sentence of the tool is here. `%1` and `%2` stand for what the command fills in: a file name, a reason, a
licence number. `tools/ci/check_messages.py` holds this page to `tools/licence/src/messages.hpp`, so a sentence added to
the code fails the check until it is described here.

**Every command**

| Sentence | When |
| --- | --- |
| `NOTE: --test uses the throwaway key test-rfc8032-1 of RFC 8032. No ARINC Data Loader program accepts what it signs.` | The first line of the output of any command given `--test`. The key's private half is the one printed in RFC 8032, so it protects nothing, and `signed_file::product_trust_list` never returns it. |
| `Run adl_licence_owner --help for the commands.` | After a usage error |
| `give a command` | No command was given |
| `unknown command %1` | The first word is not one of the five |
| `unknown option %1 for %2` | An option the command does not take |
| `%1 needs a value` | An option that takes a value was last |
| `%1 was given twice` | An option that may be given once was given twice |
| `%1 needs %2` | A required option is missing |
| `%1 needs one file` | `inspect` or `verify` was given no file, or two |
| `%1 takes no file name` | A word that is not an option, for a command that takes none |
| `--test cannot be used with --key-id or --key-file` | Both ways of naming a key |
| `%1 needs --test, or --key-id and --key-file` | No key was named |
| `%1 is not a time like 2026-11-13T02:10:00Z` | A time option that is not one |
| `%1 is not a key id: use lower-case letters, digits and hyphens, 64 characters at most` | A key id in another form |
| `%1 is not a purpose: use licence or revocations` | `keygen --purpose` |
| `an argument is not text that can be used as a file name` | Windows only: an argument has half of a surrogate pair in it, which is not text |
| `unexpected failure: %1` | The standard library threw (memory ran out, say). Nothing else is expected to. |

**Reading and writing files**

| Sentence | When |
| --- | --- |
| `cannot read %1: %2` | A file could not be read; `%2` is one of the reasons below, or the library's: the file is damaged or is not a signed file |
| `cannot write %1: %2` | A file could not be written |
| `it does not exist` | A reason |
| `it is not a file` | A reason |
| `it could not be read` | A reason |
| `it is larger than %1 bytes` | A reason: 64 KiB for a licence file, a request or a list |
| `it exists already, and the tool never writes over a file` | A reason: the output or a key file is there already |
| `it could not be written` | A reason |

**inspect and verify**

| Sentence | When |
| --- | --- |
| `verifies` | The value of `Signature` |
| `not checked: no key with this id and purpose is known` | The value of `Signature`: no key was given or built in for it |
| `does not verify: %1` | The value of `Signature`; `%1` says why (the signature, the purpose, the key's window) |
| `not readable: %1` | The value of `Payload`: the signature was looked at, and the payload is not one this version reads |
| `this tool does not know what a file for the purpose %1 holds` | A signed file that is neither a licence nor a revocation list |
| `the revocation list %1 is refused: %2` | `verify --revocations` was given a list that does not verify |
| `the public key file %1 is refused: %2` | `--trust` was given a file that is not one |
| `it is not a public key file as keygen writes it (one line with key_id, purpose and public_key)` | A reason for the sentence above |

**issue and revocations**

| Sentence | When |
| --- | --- |
| `the request %1 is refused: %2` | The request cannot be signed; `%2` is one of the reasons below |
| `the library does not read it as a request (%1)` | The library's reason: the file is damaged, a field is missing, a date is not one, a name in `critical` is not known |
| `the product is %1, and a licence from this tool is for adl` | Without `--test`, a product other than `adl` |
| `the licence number %1 is not of the form ADL-2026-000123` | A number in another form |
| `the licence number %1 is in the range 900000 to 999999, which is kept for test licences` | Without `--test` |
| `the sequence number is 0, which is the empty list every release carries: use 1 or more` | A revocation list |
| `the request could not be written as a signed file: %1` | The library refused to write the payload |
| `the key id %1 begins with test-, which is only for --test` | A key named `test-...` |
| `the key file %1 is refused: %2` | The key file could not be read, or is not a key |
| `it does not hold a key as keygen writes it` | A reason for the sentence above; the content of the file is never shown |
| `Dry run: nothing was signed and nothing was written.` | The last line of `--dry-run` |
| `Wrote: %1` | The file was written and read back |
| `the file written to %1 was removed, because reading it back failed: %2` | The check after signing failed |
| `it does not say what the request says` | A reason for the sentence above |

**keygen**

| Sentence | When |
| --- | --- |
| `the folder %1 does not exist: make it first, on the drive that will keep the key` | |
| `the folder %1 is inside a git repository: a key made there could be committed by mistake. Choose a folder that no repository holds` | |
| `this computer gives no random numbers: no key was made` | |
| `the random numbers were not usable: no key was made` | The 32 bytes were all zero or were the test key |
| `the seed file %1 is refused: %2` | `--seed-file` named a file that could not be read, or that holds the wrong thing |
| `it is not exactly 32 bytes: the key is those 32 bytes and nothing else, as a generator of your own makes them` | A reason for the sentence above |
| `the 32 bytes are all zero or are the test key, which no real key may be: no key was made` | A reason for the sentence above |
| `Key source: the system's random numbers (std::random_device)` | A line of the output, when there is no `--seed-file` |
| `Key source: the seed file %1, which was read and not changed` | A line of the output, with `--seed-file` |
| `The seed file is the same key as the private key file. Keep it as carefully, or destroy it.` | Said after it |
| `the key files were removed, because %1` | A file could not be written or read back |
| `the key file written could not be read back` | A reason for the sentence above |
| `Private key: %1` | Where the private half is |
| `It is not shown here and is never logged. Keep it offline, with two encrypted copies on separate media.` | Said after it |
| `Never put it in a repository, an email or a chat. Whoever holds it can make licences that programs accept.` | Said after it |
| `Public key file: %1` | Where the public half is |
| `It is not secret. Keep it with the licence records.` | Said after it |
| `To make programs accept licences signed with this key, add its public half to builtin_keys() in` | The three lines that follow the key's `Key`, `Purpose`, `Public key` and `Public key (hex)` lines |
| `libs/licence/src/trusted_keys.cpp, with this key id, this purpose and the public key above, and build a release.` | |
| `Make a standby key now with the same command and another id, and add its public half to the same list, so that a leak of this key can be answered at once.` | |

## The test key and the test licences

`--test` signs and checks with the key of test vector 1 of RFC 8032, whose private half is printed in the RFC and is in
`libs/licence/src/test_keys.cpp`. Its key id is `test-rfc8032-1`. It protects nothing, which is the point: the tests of
this repository and of the Pro tools need a key. `signed_file::product_trust_list` never returns a key whose id begins
with `test-`, so no program accepts a file it signs, and every output of a command run with `--test` says so in its first
line. A file signed with it says so for itself: `inspect` prints `Test key: yes` for any such file.

The tool is the same program with and without `--test`. The option is not a build switch, because the pipelines of the
private repository run the tool they build with `--test` to check their own records.

**A licence for each state.** `tests/licence/data/requests` holds a request for each test licence, and
`tests/licence/data/licences` the files the tool makes from them: `.adllic` for a licence and `.adlrev` for a
revocation list, with licence numbers in the test range (`ADL-2026-9000NN`). `manifest.txt` there says, for each case,
which file is judged when and where, and the state and the reason it is in; it holds a row for each of the eight states.
Later tests read it, and `licence.owner_tool` judges every row with `verify`.

| File | What it is |
| --- | --- |
| `valid`, `expiring`, `grace`, `expired`, `not_yet_valid` | An annual Pro licence in each position against its term. |
| `clock_suspect` | A valid licence judged when the clock is months behind the latest time seen. |
| `trial` | 14 days, no grace; counts as ending soon for its last 3. |
| `enterprise` | Enterprise, a site licence, with `data_packs`, a version window and a licensee whose name is outside ASCII. |
| `clock_reset`, `renewal` | A licence with a clock reset token, and one that replaces another. |
| `machines` | A licence for named computers, with a name that ends in `*`. |
| `revoked`, `revocations_1`, `revocations_2` | A licence and two revocation lists, the second with a higher sequence number and a withdrawn key. |
| `wrong_product` | Signed for the product `vss`, which a program of this product refuses. |
| `bad_signature`, `malformed`, `newer_version`, `unknown_key` | `valid` with one fault each: a character of the signature changed, the file cut short, an envelope that says it is format 2, a key id no program knows. |

Ed25519 signing is deterministic, so the same request gives the same file, byte for byte, on every system. To make them
again after a request changes:

```sh
cmake --build build/<preset> --target licence_test_licences
```

which runs `python3 tests/licence/make_test_licences.py --tool <adl_licence_owner>`. The test
`licence.owner_tool` runs the same script with `--check`: it makes them in a scratch folder and compares them with the
committed files, so a request or a change of the tool that alters a byte fails the test until the files are written
again and committed. `.gitattributes` marks the files `-text`, so that no checkout changes a line end.

## Arguments and file names are UTF-8

On Windows the C runtime hands a program its command line as UTF-16, and a `main(int, char**)` would get it in the
code page, which cannot hold every name. The tool's entry point there is `wmain`
(`tools/licence/os/windows/entry.cpp`), which converts each argument to UTF-8 and refuses half of a surrogate pair. On
Linux and macOS the entry point is `main` (`tools/licence/os/posix/entry.cpp`) and passes the bytes on. CMake picks the
file by the platform's name (`tools/licence/os/<platform>.cmake`), as it picks `libs/platform/<name>/`: there is no
preprocessor conditional and no operating system header in the tool ([Platform isolation](platform-isolation.md)). Paths
are built from `std::u8string`, so they are UTF-8 on every system, and what the tool prints is UTF-8 (a Windows console
that is not set to UTF-8 shows it in its own code page; a pipe or a file gets the bytes).

## The package check

`tools/release/check_package_contents.py` fails a package that holds the tool, a library it signs with or a key file:

```sh
python3 tools/release/check_package_contents.py arinc-data-loader-0.1.0-linux-x86_64.tar.gz
python3 tools/release/check_package_contents.py --directory <an unpacked package>
```

It looks at every file of a `.zip`, a `.tar.gz` or a folder, for

- the **names** `adl_licence_owner`, `adl_licence_owner_core`, `adl_licence_sign` and `adl_licence_test_keys` (any
  extension, with or without `lib`, or as a macOS bundle), the headers `adl/licence/sign.hpp` and
  `adl/licence/test_keys.hpp`, and the names of key files: `*.adlownerkey` and `*.adlownerpub` (what `keygen` writes),
  `*.adlkey` (a customer's own site key, which another tool writes), `*.key`, `*.pem`, `*.p12`, `*.pfx`, `*.priv`,
  `id_ed25519` and the like;
- the **bytes**, whatever the name: the text `adl_licence_owner` (a renamed copy of the tool), the private half of the
  RFC 8032 test key (raw, in hexadecimal, in base64 and in base64url), and a file that is nothing but 43 characters of
  base64url, which is what `keygen` writes.

It exits 1 and names each file it found, 2 for a package it could not read (it passes nothing it did not read), and 0 for
a clean one. `tools/release/Test-Package.ps1` runs it on every package it unpacks, and the release workflow runs
that, so a release with one of them in a package fails ([Releasing](releasing.md)). The tests of the check are in
`tests/licence/test_package_check.py`; they fail each kind of finding as a `.zip`, a `.tar.gz` and a folder, and pass a
clean package. The development package of a Release build is also checked as it is installed
(`tests/packaging/check_install.cmake`).

## Tests

| Test | What it holds |
| --- | --- |
| `tests/licence/owner_test.cpp` (`adl_licence_owner_tests`, `unit`) | UTF-16 to UTF-8 (pairs, the edges, every way a surrogate can be alone), paths and files with names outside the code page, the form of a licence number, a key id and a public key file (every way it is refused), the options, the sentences' placeholders, the git work tree check, a signed file read back and removed when the check fails, and the commands run against streams: help, issue, verify, a request that is not a licence, keygen's refusals (a folder in a work tree, a seed file of the wrong size, zeros or the test key), a key file that is not a key |
| `tests/licence/test_owner_tool.py` (`licence.owner_tool`, `system`) | The built program as a process: every command and every usage error, the test note and the `Test key` line, the manifest of test licences (a row for each of the eight states) judged by `verify`, the revocation lists, every refusal of `issue` leaving nothing behind, never writing over a file, a key made (from the system's random numbers and from a seed file, which is refused when it is not 32 bytes of a key), used to sign and checked with its public file, the wrong purpose, a public key file in every wrong form, names outside the code page for a key folder, a request and a licence, and that the private half of the test key never appears in any output |
| `tests/licence/test_package_check.py` (`licence.package_check`, `system`) | The package check, described above |
| `tools/licence/fuzz/owner.cpp` (`fuzz.regress.owner`, `fuzz`) | What the tool reads that is not a licence file: a public key file (what is accepted is what the writer writes, and reads back the same), the words of a command line, UTF-16, a licence number and a key id. The seeds are made by `tools/fuzz/make_seeds.py` ([Fuzzing](fuzzing.md)) |

A key is made only in a scratch folder the test makes and removes, outside any git work tree: CTest points `TEMP` at the
build folder, which is inside the checkout, so the test looks for another temporary folder. No real key exists in the
repository, and no test writes one anywhere else.
