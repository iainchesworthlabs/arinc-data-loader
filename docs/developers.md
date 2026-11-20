# For programmers

This section is for people who build the software, use its libraries in a program of their own, or want to know how it is checked. The pages for people who only run the programs are in [the user's guide](user-guide/index.md).

## On this site

| Page | What it is |
| --- | --- |
| [Using the libraries](libraries.md) | The development package, the `adl` CMake package and its targets |
| [Architecture](architecture.md) | The libraries, the programs and how they depend on each other |
| [Platform abstraction](platform-abstraction.md) | The interfaces to files, sockets, threads, memory and time, and what each platform has verified |
| [Platform isolation](platform-isolation.md) | How CMake, not the source, chooses the platform, and how that is enforced |
| [ARINC 665-3 coverage](arinc665-coverage.md) | The media set standard, section by section, with the tests and the unclear places |
| [ARINC 615A codec](arinc615a-codec.md) | The packet and file layouts |
| [ARINC 615A engine](arinc615a-engine.md) | The protocol state machines and the runner |
| [ARINC 615A-3 coverage](arinc615a-coverage.md) | The loading protocol standard, section by section |
| [Where the standards go](specs/README.md) | The two standards are not in the repository |
| [Building](building.md) | Requirements, presets and options |
| [Testing](testing.md) | The test tiers, coverage and what each test holds down |
| [Fuzzing](fuzzing.md) | The libFuzzer harnesses of the decoders |
| [Security and threat model](security.md) | What untrusted data the programs read and what is done about it |
| [Licensing](licensing.md) | The GNU General Public License, the third-party components and their notices |
| [Roadmap](roadmap.md) | What is built and what is planned |

## In the repository only

These pages are for the people who work on the project. They are on GitHub, next to the code they describe.

| Page | What it is |
| --- | --- |
| [The application](app.md), [the media set generator](media-gen.md), [the mock target](mock-target.md) | Each program, for developers |
| [CI](ci.md), [Static analysis](static-analysis.md), [Performance](performance.md), [Profiling](profiling.md) | What is run on every change and every night |
| [Releasing](releasing.md), [Repository settings](repository-settings.md) | How a release is made and how the repository is set up |
| [Design system](design-system.md), [Design overhaul](design-overhaul.md) | The look of the three windows and the analysis behind it |
| [Translations](translations.md), [Translation glossaries](translation-glossaries.md) | The eight languages and how a language is added |
| [Screenshots](screenshots.md) | How the pictures in the guide are made |
