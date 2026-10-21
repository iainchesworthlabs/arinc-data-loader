# ARINC Data Loader

ARINC Data Loader implements two standards for loading software into aircraft equipment: ARINC 665, which
describes how loadable software is packaged into a *media set*, and ARINC 615A, which describes how a data
loader moves a load to (and from) a target over an Ethernet network. It is two libraries and three programs,
for Windows, Linux and macOS:

| | What it is |
| --- | --- |
| **ARINC Data Loader** (`adl_app`) | Finds a target on the network, reads what it holds, sends a load to it (*upload*) and fetches files from it (*download*) |
| **ARINC Media Set Generator** (`adl_media_gen`) | Turns files from your disk into a media set, the folder layout the loader reads, and checks what it wrote |
| **ARINC Mock Target** (`adl_mock_target`) | Pretends to be a piece of equipment, so you can practise and test without hardware |
| **The libraries** | ARINC 665 media sets, the ARINC 615A codec and state machines, and the platform layer under them, for programs of your own ([Using the libraries](libraries.md)) |

## Status

Version 0.1.0 is a preliminary release. The libraries and the three programs work end to end against each
other on one machine. **Nothing has been checked against a real target, a real loader or another tool's media
sets.** [What has been checked](status.md) says what has been and what has not.

## Get it

Each release has a portable package for Windows, Linux and macOS, with checksums, signatures and a bill of
materials: [the releases page](https://github.com/iainchesworthlabs/arinc-data-loader/releases),
[Installing and starting the programs](user-guide/installing.md) and
[Verifying a release](verifying-releases.md). To build from source, see [Building](building.md).

## How the pieces fit

```text
  files on disk ──► Media Set Generator ──► media set (a folder tree)
                                                     │
                                                     ▼
     Mock Target ◄── FIND / Information / Upload / Download ──► Data Loader
     (pretends to be equipment, keeps what you upload, serves files to download)
```

![The data loader after an upload to the mock target](images/app-upload-complete.png)

## Where to start

| I want to... | Read |
| --- | --- |
| Try it on one computer, with no hardware | [The demonstration](user-guide/demonstration.md) (about ten minutes) |
| Make a media set from my files | [Make a media set](user-guide/make-a-media-set.md) |
| Load or download from a target | [The loader, screen by screen](user-guide/loader.md), then [Find a target](user-guide/find-a-target.md), [Upload](user-guide/upload.md) and [Download](user-guide/download.md) |
| Know what a word means | [Glossary](glossary.md) |
| Find a quick answer | [Questions and answers](faq.md) |
| See which files the programs read and write | [Files the programs read and write](file-reference.md) |
| Use the libraries in my own program | [Using the libraries](libraries.md) |
| Build, test or change the software | [Building](building.md), [Testing](testing.md) and [CONTRIBUTING](../CONTRIBUTING.md) |
| Report a bug or a security problem | [The issue tracker](https://github.com/iainchesworthlabs/arinc-data-loader/issues) and [SECURITY](../SECURITY.md) |

## All the pages

Everything here is in the repository, next to the code it describes, and is published as a website at
<https://iainchesworthlabs.github.io/arinc-data-loader/> from the first release on ([how](ci.md#documentation-site)).
The project overview is the [README](../README.md); what changed is the [changelog](../CHANGELOG.md).

## For people who use the programs

| Page | What it is |
| --- | --- |
| [User's guide](user-guide/index.md) | Start here: what ARINC 615A and 665 are, installing, a ten-minute demonstration, every screen and setting, troubleshooting, where files go and what the programs cannot do |
| [Verifying a release](verifying-releases.md) | For people who download a release: the checksums, the PGP signatures, the build attestations and the bill of materials, and how to check them |
| [Screenshots](screenshots.md) | How the pictures in [`images/`](images) are made again from the real programs |

## For people who build, test and change the software

| Page | What it is |
| --- | --- |
| [Building](building.md) | Requirements, presets, options, the demonstration, sample payloads |
| [Testing](testing.md) | The three test tiers, the UI suite, coverage, what each test holds down |
| [CI](ci.md) | What each continuous-integration job proves, and which are required |
| [Static analysis](static-analysis.md) | The nightly analysers, how to run them, the suppression rules |
| [Profiling](profiling.md) | The opt-in Tracy build |
| [Branch protection](branch-protection.md) | The repository rules and rulesets |
| [Releasing](releasing.md) | For the maintainer: what a release contains, how it is cut, built, signed and published, and the one-time repository setup |
| [Licensing](licensing.md) | The GPL, the third-party notices, how to add a component |
| [Roadmap](roadmap.md) | What is built, what is planned, in order |

## The design of the software

| Page | What it is |
| --- | --- |
| [Architecture](architecture.md) | The libraries, the programs and how they depend on each other |
| [Platform abstraction](platform-abstraction.md) | The interfaces to files, sockets, threads, memory and time, and what each platform has verified |
| [Platform isolation](platform-isolation.md) | How CMake, not the source, chooses the platform, and how that is enforced |
| [Green Hills INTEGRITY](platform-ghs.md) | The review behind the two INTEGRITY ports (unbuilt) |
| [ARINC 665 coverage](arinc665-coverage.md) | The media set standard, section by section, with the tests and the unclear places |
| [ARINC 615A codec](arinc615a-codec.md) | The packet and file layouts |
| [ARINC 615A engine](arinc615a-engine.md) | The protocol state machines and the runner |
| [ARINC 615A coverage](arinc615a-coverage.md) | The loading protocol standard, section by section |
| [The application](app.md) | The loader, for developers: threads, models, what it does and does not check |
| [The media set generator](media-gen.md) | The generator, for developers |
| [The mock target](mock-target.md) | The mock target, for developers |
| [Design overhaul](design-overhaul.md) | The human-factors analysis behind the three windows, before and after |
| [Design system](design-system.md) | The shared look: tokens, components, keyboard |
| [Translations](translations.md) | The eight languages, how a language is switched and the window mirrored for Hebrew and Yiddish, adding a string, the typefaces; [the glossaries](translation-glossaries.md) the drafts chose |
| [Specifications](specs/README.md) | Where the standards go (they are not in the repository) |

The pictures are in [`images/`](images) (the current windows) and [`design/screenshots`](design/screenshots)
(the before-and-after record of the design overhaul).
