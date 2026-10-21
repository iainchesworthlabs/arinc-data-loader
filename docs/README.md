# Documentation

Everything here is in the repository, next to the code it describes. The project overview is the
[README](../README.md); what changed is the [changelog](../CHANGELOG.md).

## For people who use the programs

| Page | What it is |
| --- | --- |
| [User's guide](user-guide/index.md) | Start here: what ARINC 615A and 665 are, installing, a ten-minute demonstration, every screen and setting, troubleshooting, where files go and what the programs cannot do |
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
