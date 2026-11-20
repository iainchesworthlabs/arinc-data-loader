# Limits and known issues

This page lists the limits of the programs and the problems that are known. It was reviewed on 19 November 2026 against the version named at the top of the page. A limit that comes from a standard says which. [What has been checked](../status.md) says what has been tried and what has not, and [Safety of use](safety-of-use.md) says what not to rely on.

## Limits

| Thing | Limit | Where it comes from |
| --- | --- | --- |
| Members in a media set | 1 to 255 | ARINC 665-3 |
| File name in a media set | 255 characters | ARINC 665-3, section 2.2.2 |
| File name you ask a target for in a download | 254 characters | The loader |
| Folders deep inside a member | 4 (a deeper file is a finding) | ARINC 665-3 |
| Part number of a media set | 15 characters | ARINC 665-3 |
| Part number of a load or a batch | Exactly `MMMCC-SSSS-SSSS` | ARINC 665-3 |
| Loads per media set the generator makes | 1 (the loader reads sets with more) | The generator |
| File size in the generator | What fits in memory: each file is read whole | The generator |
| Media set size in the loader | Choosing or re-checking a set reads every file of it whole into memory. An upload reads the files again from disk, in blocks, as the target asks | The loader |
| Files the loader keeps open at once | 8 | The loader |
| Targets one FIND lists | 64 | The loader |
| Files named in one media defined download | 256 | The loader |
| `THW_ID_POS` | 1 to 240 characters, none a control character, a slash or a backslash. For a download also printable ASCII only, and none of colon, asterisk, question mark, quote, angle bracket or bar, because the download folder is named after it | The loader |
| Mock target FIND answer | Hardware ID 15 characters, type name 8, position 8, literal name 20, manufacturer code 3 | ARINC 615A-3, Attachment 3 |
| Activity log lines | 100 to 1 000 000 (default 2000) | The loader |
| Licence files | At most 64 KiB each, and 64 files read from each of the two licence folders, in name order | The licence code ([where licence files go](licence.md#where-licence-files-go)) |
| Ports | 1 to 65535 | UDP |
| Time-out for a packet | 2 seconds, and one retry | ARINC 615A-3, Attachment 4 |
| Wait for a status file | 13 seconds | ARINC 615A-3, Attachment 4 |
| FIND listens for | 3 seconds | ARINC 615A-3, Attachment 3 |
| Jobs at a time | One. While the loader runs a FIND, an operation or the reading of a media set, it refuses another | The loader |
| Batch files | The loader reads and counts them and does not use them. An upload sends only the loads you tick | The loader |

The numbers in the 8, 64, 240, 256, FIND-answer, time-out and port rows were checked against the code (`libs/arinc615a/include/adl/arinc615a/`, `libs/arinc615a_runner/include/adl/arinc615a/`) when this page was reviewed. [Where files go, and limits](files-and-limits.md) has the rest of the detail.

## What the programs cannot do

- They do not run on the equipment. There is no program for the target side. The mock target is not a target.
- FIND does not reach a target on the same computer, does not cross routers, and cannot work on a network with no broadcast, such as AFDX. A firewall can block it. Type the address instead ([Find a target](find-a-target.md)).
- They do not authenticate a target or encrypt anything. ARINC 615A over TFTP has neither.
- They use IPv4 and UDP only.

## Known problems

Each has a way round. Dates are the date of the source, not of a fix.

| Problem | Way round | Source |
| --- | --- | --- |
| FIND between the loader and the mock target on one computer reports "0 targets answered". The reason is not known. | Type the target's address and `THW_ID_POS`. The [demonstration](demonstration.md) does. | [Troubleshooting](troubleshooting.md#find-finds-nothing); the 0.1.0 changelog (22 October 2026) |
| In a Debug build made from source on Windows, the system's folder chooser ("Choose folder...") does not work, because Debug builds run under AddressSanitizer. A release package is not affected. | Build a Release configuration, or use a package. | The 0.1.0 changelog (22 October 2026) |
| On Linux the mock target's ports (59 and 1001) and the loader's default control port (59) are below 1024, and the system refuses them to an ordinary program. | Set the loader's control port to 5900 in Preferences, give the program the capability, or lower `net.ipv4.ip_unprivileged_port_start` ([Installing](installing.md#linux)). | The 0.1.0 changelog (22 October 2026) |
| Seven of the eight translations are machine drafts that no speaker has read. The ARINC 665 findings, the platform's error codes, what a target itself sends and the mock target's protocol log lines stay in English. Chinese and Japanese use the system's fonts, and a system without one shows empty boxes. | Use English, or report the wording that is wrong ([Languages and colour](languages-and-colour.md)). | The 0.1.0 changelog (22 October 2026) |
| The loader reads a batch file and counts it, and does nothing more with it. | Tick the loads to send. | [Where files go, and limits](files-and-limits.md#limits) |
| A nightly CodeQL run for the C++ code failed to build or to complete on 14 November 2026. It is an analysis run, not a fault in a program. | None needed to use the programs. | The project's issue 77 on GitHub |

## Not checked

[What has been checked](../status.md) has the full list. The points that most often matter:

- Nothing has been checked against real equipment, another vendor's loader or target, or another tool's media sets.
- Where the standards are unclear, the choices are the project's own. The ones most likely to differ from a real device are the order of the files in the Load CRC (ARINC 665), the text length of a protocol file, the status codes, the checksum option's value and the FIND manufacturer code (codec), and the time-outs applied to the `.LNO`, `.LCL` and `.LNL` files (engine). The lists are in the coverage pages ([ARINC 665-3](../arinc665-coverage.md#places-the-standard-is-unclear), [ARINC 615A codec](../arinc615a-codec.md#places-the-standard-is-unclear), [ARINC 615A-3](../arinc615a-coverage.md#places-the-standard-is-unclear)).
- How a target's hardware ID and position become its `THW_ID_POS` follows the standard's examples and has not been confirmed with a real target. The field can be edited.
- Which check value a real target asks for each file of an upload.
- A media set of many gigabytes, a very slow or lossy network, and a target busy with another loader.
- FIND with more than one network adapter, or across routers.
- Windows is the only system on which the programs have been driven by hand. Linux and macOS are built and tested in continuous integration, and nobody has driven the windows on them.
- Where the settings are kept on Linux and macOS.
- Screen readers, and display scales other than 100 percent ([Accessibility](../accessibility.md)).
