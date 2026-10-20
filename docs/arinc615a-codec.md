# ARINC 615A codec

The codec half of `adl::arinc615a` (`src/arinc615a`) turns the packets and files of
ARINC 615A-3 into bytes and back: the TFTP packets with their options, the FIND packets, and the
twelve protocol files of section 6.4. It does not send, receive or keep time. The protocol
engine that uses it (the operations, the state machines, the time-outs) is in the same library,
under other headers, and is described on [the next page](arinc615a-engine.md#shape); this page
says what the codec does, where the standard was unclear and what was chosen, and what has and
has not been checked.
Section numbers are 615A-3's, which is not in the repository
([docs/specs](specs/README.md)).

[The coverage page](arinc615a-coverage.md), in the form of [the 665
one](arinc665-coverage.md), maps the whole standard onto the codec and the engine.

## Shape

| Header | What it holds |
| --- | --- |
| `tftp.hpp` | TFTP packets (RRQ, WRQ, DATA, ACK, ERROR, OACK), block numbers, the options and a builder for them, the WAIT and ABORT messages |
| `find.hpp` | The FIND request and answer |
| `protocol_files.hpp` | `.LCI .LUI .LND .LNO`, `.LCL`, `.LCS`, `.LUR`, `.LUS`, `.LNR`, `.LNS`, `.LNL`, `.LNA`, their names and the Protocol Version |
| `status_code.hpp` | The status codes of Table 6.4.10-1 and which field may hold which |
| `limits.hpp` | The ports, block sizes, text limits and other figures the codecs check against |
| `list.hpp` | `List<T>`, how a file's entries are held (below) |
| `error.hpp` | `Error` and `Result<T>` (`std::expected<T, Error>`) |

The library is written for an embedded target as well as a PC (see the [embedded
direction](architecture.md#the-embedded-direction) and [platform
abstraction](platform-abstraction.md#area-2-memory-memoryhpp)), and its shape follows from that:

- **Every encoder writes into a span the caller supplies** and returns how many bytes it
  wrote. `encoded_size()` runs the same checks and says how large the buffer must be, so a
  caller can size it exactly, or pick a bound it knows (`max_packet_size()`,
  `find_max_answer_size`). A buffer that is too small is `Error::buffer_too_small`, never
  a write past its end. An encoder that fails for another reason reports that reason first.
- **Every decoder returns views over the bytes it was given.** Texts are `std::string_view`,
  data is `std::span<const std::byte>`, and the entries of a file are a `List<T>` that reads
  them from the bytes as they are walked, after the decoder has checked the lot. So decoding
  allocates nothing, and the decoded value is valid only while the bytes are. The same
  `List<T>` is built from a `std::array` or `std::vector` on the way out, so one type serves
  both directions, and a decoded file can be encoded again as it is.
- **Nothing allocates, throws or needs RTTI.** There is no `iostream`, no `std::string` or
  `std::vector` in the library, every function is `noexcept` and failures are values. The only buffer is `OptionBuilder`'s own 256 bytes. (The one allocation in the library is in
  the engine, not the codec: calculating a checksum, see [the engine
  page](arinc615a-engine.md#memory-buffers-and-lifetimes).) The codec builds no `adl::arinc665`
  model and uses that library for the check value numbers (`CheckValueType`) only; the engine's
  `checksum.hpp` also uses its `IntegrityChecker`.
  What is *not* checked yet is the build under `-fno-exceptions -fno-rtti`: that is a CI job
  on the roadmap.
- **A decoder checks lengths before it believes them.** A count is trusted only as far as
  the bytes that follow it go, so a file claiming 65535 entries in ten bytes is `truncated`
  after one step, not a loop of 65535. `tests/unit/arinc615a_robustness_test.cpp` runs every
  decoder over every truncation and bit change of a good file and over random bytes under
  the sanitizers of the Debug build.

## TFTP (section 5.3.2)

Packets are RFC 1350's, octet mode only, with the option extension of RFCs 2347, 2348 and
2349 and the two options 615A adds.

| Opcode | Packet | Layout |
| --- | --- | --- |
| 1, 2 | RRQ, WRQ | opcode, file name, 0, `octet`, 0, then option name, 0, value, 0 for each option |
| 3 | DATA | opcode, block number, 0 to blksize bytes of data |
| 4 | ACK | opcode, block number |
| 5 | ERROR | opcode, error code, text, 0 |
| 6 | OACK | opcode, then option name, 0, value, 0 for each option accepted |

- **The mode** is always `octet` on the way out, and is read without regard to case. Any
  other mode is `Error::unsupported_mode`.
- **Block numbers** are 16 bits, the first DATA block is 1, a WRQ (and an OACK to one) is
  acknowledged with block 0, and after 65535 comes 1 (5.3.2.3.9): `next_block()`. The codec
  decodes any block number, 0 included; checking that the number is the expected one is the
  engine's.
- **A DATA packet** holds up to 65464 bytes, the largest block size RFC 2348 allows.
  A longer one is `Error::bad_length`.
- **Options.** `read_transfer_options()` reads the ones the library knows from the options
  of a request or OACK and ignores the rest, as 5.3.2.3.8 asks: `blksize` (8 to 65464),
  `tsize` (any 64-bit size; a read request says 0), `timeout` (1 to 255 seconds), `port`
  (1 to 65535) and `checksum_<n>`. Names are compared without regard to case. A known option
  that is repeated, not a plain decimal number, or out of range is `Error::bad_option`.
  `OptionBuilder` makes the options of a request, formatting the numbers into a buffer of its
  own.
- **The checksum option** (5.3.2.3.8.5) is named for the algorithm: `<n>` is the number in
  ARINC 665-3's Table 5.1 (1 = 8-bit CRC to 5 = SHA-1, `adl::arinc665::CheckValueType`).
  See the choices below for its value. An algorithm number the table does not have is an
  option the library does not know, and is ignored. Validating a file against the checksum
  is the engine's job and, by 5.3.2.3.8.5, must not fail the transfer.
- **WAIT and ABORT** (5.3.2.3.4 to 5.3.2.3.7) are error packets with error code 0 whose text
  starts `WAIT:` and a number of seconds (0 to 65535), or `ABORT:` and four hexadecimal
  characters, a status code of Table 6.4.10-1. `encode_wait()` and `encode_abort()` write
  the whole packet; `interpret_error()` says what an ERROR packet is: a `WaitMessage`, an
  `AbortMessage`, or an ordinary `TftpError`. Text that starts with `WAIT:` or `ABORT:` but
  has no good number after it is `Error::invalid_value`, not an ordinary error, so that the
  engine can tell a broken 615A message from a plain failure. The same texts under another
  error code are ordinary errors.

## FIND (Attachment 3)

A request is the opcode 0x0001, one NUL and the byte 0x10. An answer is the opcode 0x0002,
five NUL-terminated strings (the Target Hardware Identifier, Target Type Name, Target
Position, Literal Name and Manufacturer Code, with at most 15, 8, 8, 20 and 3 characters)
and 0x10. `encode_find_request()`, `encode_find_answer()` and `decode_find()`. FIND has no
error replies, so a request or answer that does not decode is simply ignored by the engine.
An answer never exceeds `find_max_answer_size` (62 bytes).

## The protocol files (section 6.4)

All twelve start with the File Length (32 bits, in bytes, counting itself) and the
Protocol Version (the two ASCII bytes `A4`), and all numbers are big endian.

| Extension | Section | Type | Entries | Encode, decode |
| --- | --- | --- | --- | --- |
| `.LCI .LUI .LND .LNO` | 6.4.1 | `InitializationFile` | none | `*_initialization_file` |
| `.LCL` | 6.4.2 | `ConfigurationList` | `HardwareItem`, each with its `HardwarePart`s | `*_configuration_list` |
| `.LCS` | 6.4.3 | `ConfigurationStatus` | none | `*_configuration_status` |
| `.LUR` | 6.4.4 | `UploadRequest` | `LoadReference` | `*_upload_request` |
| `.LUS` | 6.4.5 | `UploadStatus` | `LoadProgress` | `*_upload_status` |
| `.LNR` | 6.4.6 | `DownloadRequest` | `FileRequest` | `*_download_request` |
| `.LNS` | 6.4.7 | `DownloadStatus` | `FileProgress` | `*_download_status` |
| `.LNL` | 6.4.8 | `DownloadList` | `FileListing` | `*_download_list` |
| `.LNA` | 6.4.9 | `DownloadAnswer` | file names | `*_download_answer` |

A decoder returns `Error::truncated` for a file that ends early (or whose File Length is more
than it was given), `Error::bad_length` for bytes left over or a File Length that is less than
the bytes, `Error::bad_version` unless the version is `A4`, and the error of the first field
that is wrong after that. `peek_protocol_version()` reads the version field without
decoding the rest, for a data loader that is to refuse a version it does not know with an
ABORT (section 6.4). `extension()` and `kind_of_file_name()` map the twelve kinds to their
(case sensitive) extensions.

## Places the standard is unclear

The choices below are mine. Each is isolated enough to change in one place, and each is a
reason to check the codec against a real target or loader. The independent writer
(`tests/golden/make_golden_615a.py`) shares them.

1. **What a text field's length counts.** The tables give an 8-bit length before each text
   and the text "zero terminated". Section 6.4's commentary says the length is the size of
   the field, which the text need not fill, and that an implementer may set it to the text
   plus its terminator, or to zero for no text. This library writes the length as the
   characters plus the NUL (so `Busy` is `05 42 75 73 79 00`) and an empty optional text as a
   lone `00`. It reads a field of that many bytes, takes the text up to the first NUL (or the
   whole field if there is none) and ignores any padding after the NUL. The consequence is
   that a text is at most **254** characters, though the tables say 255 (the 255th would put
   the length at 256). A target that counts the characters only, with the NUL after them,
   would be misread; File Length would then fail to agree and the file is refused, not
   misinterpreted. A field of 255 bytes with no NUL decodes as 255 characters and cannot be
   written back.
2. **Which texts may be empty.** The names, part numbers, serial numbers and designations are
   "1 to 255 characters": the codec refuses an empty one (`Error::invalid_value`). Status
   descriptions, amendments, file descriptions and user data may be empty.
3. **Control characters.** The standard says texts should be printable and have no control
   characters. The codec refuses them (`Error::malformed_text`) in both directions: a target's
   text goes to an operator's screen, and a terminal escape in it should not get there. Bytes
   above 0x7F are let through, since the standard says only "8-bit ASCII".
4. **The tables as printed.** In the PDF's text the field sizes of Tables 6.4.1-1 and
   6.4.6-1 come out scrambled; the sizes used (an 8-bit Status Description Length in
   6.4.1; an 8-bit name length, an 8-bit user data length and the data per file in 6.4.6) are the
   ones read from the table's own cells and the field descriptions under it. The user
   defined data of an entry in `.LNR` is up to 255 bytes, not text.
5. **The status codes.** Table 6.4.10-1 lists ten codes: 0x0001 to 0x0004, 0x1000, 0x1002 to
   0x1005 and 0x1007 (0x1001 and 0x1006 are not defined). 0x1007 is "load of a header file
   failed" in a `.LUS` entry and "download of a file failed" in a `.LNS` entry, one code in
   two rows. The decoders accept in each field only the codes that section 6.4.x lists for
   it: an acceptance answer is 0x0001, 0x1000 or 0x1002; the status of a whole operation is
   0x0001 to 0x0004 or 0x1003 to 0x1005; one load or file may also be 0x1007. Anything else is
   `Error::invalid_value`. The display texts of the table are the application's; the
   codec does not carry them.
6. **Fields the standard says must be 0.** For a status other than 2 and 4 the exception
   timer and the estimated time are to be 0, and the estimated time is 0xFFFF when the
   target cannot say. Decoders return what the file holds; the standard's wording is
   about what a target sends, not a reason to refuse its status file. The text of 6.4.3
   gives the estimated time as "-1 to 2^15-1" and then 0xFFFF; it is carried as a 16-bit
   figure, with `estimated_time_unknown` for 0xFFFF.
7. **The ratios** (Load List Ratio, Load Ratio, Download List Ratio) are 24 bits of "three
   characters, right adjusted with leading blanks". They are a `std::uint8_t` of 0 to 100,
   written as `"  7"`, `" 45"`, `"100"`; read from blanks followed by digits (leading zeros
   too), and refused if over 100, all blank, or anything else.
8. **At least one load.** A `.LUR` with no loads is refused both ways (6.4.4 says it should
   hold at least one). Every other list may be empty.
9. **The checksum option's value.** 5.3.2.3.8.5 says "a string representation depending on
   the selected checksum algorithm" and no more. The value is the check value as ARINC 665
   stores it (a 16-bit CRC is two bytes, a CRC-8 also two, a CRC-32 four, MD5 16, SHA-1 20)
   written as hexadecimal characters, capitals on the way out, either case on the way in. The
   wrong number of characters, or a character that is not hexadecimal, is `Error::bad_option`.
   More than one checksum option in a packet is refused too.
10. **The port option's range.** 5.3.2.3.8.6 says a port from 49152 up *should* be used. The
    standard's own example in Appendix G asks for 1615. The codec takes 1 to 65535;
    `first_private_port` is there for the engine to default to.
11. **The FIND manufacturer code.** The size limits say 3 characters. The example in 3-7 prints
    `7HNP` (four). The limit is followed. A FIND packet must end with the byte 0x10 and no
    byte may follow it; a request's data list must be the single NUL. Control characters
    in the strings are refused (0x10 among them).
12. **WAIT and ABORT texts.** The wait time is decimal digits (up to five, value up to 65535,
    no sign or space), with no leading zeros on the way out. The status code is four
    hexadecimal characters: capitals on the way out, either case on the way in.
13. **An OACK with no options** is not a packet (`Error::bad_option`), and an option with an
    empty name is refused. Unknown options, in either direction through the builder's `add()`, are
    allowed.
14. **What the standard gets wrong.** Section 5.3.2.2 sends the reader to "Section 5.3.2.3.9"
    for the options (they are in 5.3.2.3.8). The headings 5.3.2.3.8.4 (Part Number) and
    5.3.2.3.8.5 repeat the Timeout Interval paragraph, so there is no part number option and
    nothing was implemented for one. Attachment 4 lists `.LNI` among the files the DLP time-out
    applies to; there is no such file, and `.LND`/`.LNO` are meant (the engine reads it so:
    [the engine page](arinc615a-engine.md#the-data-load-protocol-time-out-attachment-4-4-41)). Appendix G's packet examples are figures of the exchanges only (the sequence, the
    addresses, port 59 and the port option 1615) and contain no packet bytes, so the independent
    check below rests on RFC 1350's and RFC 2347's layouts and on the standard's own WAIT and
    FIND examples.

## What has been checked, and what has not

Every layout and limit above is held down by `tests/unit` ([testing](testing.md)): the
standard's own numbers (the WAIT example, the FIND request and answer, the 59/1001 ports,
block size and block wrap), round trips, every truncation, bit change and byte change,
random bytes, and byte for byte against an independent writer. The independent writer is
written from the tables and shares no code with the library; it shares the choices above.

**Not checked:** against any other implementation of 615A, a loader or a target. Choices 1,
5, 9 and 11 are the first to confirm against a real device. A target that decodes a text
length in the other way (choice 1) is the likeliest difference.

**Not in the codec:** the checksum values themselves (calculating and comparing one is
`adl::arinc665`'s and the engine's job), the shape of a `<THW_ID_POS>` (an identifier, an
underscore and 0 to 8 characters), time-outs and retries (Attachment 4), the Sorcerer's
Apprentice rule, any state machine, and the language of the Display Text column of Table
6.4.10-1.
