# ARINC 665-3 coverage

This page reviews `docs/specs/665-3.pdf` against `adl::arinc665`: what each
section asks for, where the library does it, and which tests hold it down. It is
also the list of places the standard is unclear, because those are where an
interoperability test against another tool matters most.

Where a table says **error**, the library refuses the file (the standard says
"must", or the layout cannot be read without it). Where it says **finding**, the
file is read and written and the departure is reported in a `Finding` (the
standard says "should"). Section numbers are those of the 665-3 edition in
`docs/specs/`.

## In short

- Every file format in sections 2 and 3 is written and read: the header file
  (`.LUH`), the batch file (`.LUB`), and the three lists (`LOADS.LUM`,
  `FILES.LUM`, `BATCHES.LUM`). So are the CRCs of section 4, the five check values
  of section 5, the media set's file organisation (3.2.4) and the label (3.3).
- A load can be built from file contents (`build_load`), a media set from loads
  (`build_media_set`), and both read back and verified end to end
  (`verify_load`, `read_media_set`).
- Each format has been compared byte for byte with a second implementation,
  written separately from the standard's tables (`tests/golden/make_golden.py`).
  That catches a mistake in the C++; it cannot catch a misreading that both share.
  Nothing here has yet been checked against a load written by someone else's
  tool, which is the next thing to do (see [Not yet verified](#not-yet-verified)).
- The three files of a media set are read from memory. Reading and writing a
  directory on a disk is not part of the library; it needs the platform section.

## Section by section

| Section | What the standard asks | Where | Tests |
| --- | --- | --- | --- |
| 1.3.2, 3.4 | Words of 16 bits, multi-byte values most significant byte first | `wire.hpp` `WordWriter`/`WordReader` | every golden and layout test |
| 1.3.3 | Text is ASCII | strings are `std::string`, bytes as given | `header_file_test` |
| 1.4.1 | Format versions 0x8004 (load), 0x9004 (batch), 0xA004 (media) | `FileFormatVersion`; the decoders require the class and a version no earlier than these | `header_file_test`, `batch_file_test`, `media_files_test` (damage); `load_test` |
| 1.4.2 | Creators do not add fields of their own at expansion points | the encoders write none; there is nowhere to add one | by construction |
| 1.4.3 | A tool reads a later version of its class through the pointers | every decoder walks by pointer and steps over what it does not know; a later version number is accepted | `header_file_test` ("read by its pointers"), `batch_file_test`, `media_files_test` |
| 1.4.4 | Absolute pointers (words from the start, "20" for the Load PN) and relative ones (distance to the next entry, counting the pointer; 0 in the last) | header, lists and batch encoders and decoders | `header_file_test` ("fixed layout", "pointer of a list entry"), `batch_file_test`, `media_files_test` |
| 1.5 | Target hardware IDs: four hex characters (ARINC 429), or 4 to 15 beginning with a manufacturer's code | `check_target_hardware_id` | `naming_test` |
| 1.6 | Electronic distribution | the file formats are independent of the medium; packaging for distribution is not built | n/a |
| 2.1.1 | Part number `MMMCC-SSSS-SSSS`, upper-case alphanumerics, no I O Q Z in SSSS-SSSS, no blanks | `check_load_part_number`, `make_load_part_number` | `part_number_test` |
| 2.1.1 | A 615A loader does not refuse a part number for its format | `PartNumber` holds any text; compliance is a finding | `part_number_test` |
| 2.1.2 | Manufacturer's codes are assigned by ARINC | the list (Attachment 1) is not in the library, so a code is checked for form only | n/a |
| 2.1.3, App. E | Check characters: exclusive-or of the characters without delimiters, two upper-case hex digits (`ACM47-1234-5678`) | `check_characters` | `part_number_test` (the standard's example) |
| 2.2.1 | A load is a header file, one or more data files, optional support files. File names unique in a load, case-sensitive. Header and data files are whole words | `HeaderFile`, `build_load`; zero data files is an **error**; the rest are findings | `header_file_test`, `load_test` |
| 2.2.2 | File names up to 255 characters, printable, none of `~ / : \`, not `.` or `..`; the header file begins with the manufacturer's code | `check_file_name`, `check_header_file_name` | `naming_test` (see [the file name list](#places-the-standard-is-unclear)) |
| 2.2.2.1-3 | Extensions: header `LUH`; data `LUP` recommended; support any that are not reserved | `check_header_file_name`, `check_data_file_name`, `check_support_file_name` | `naming_test` |
| 2.2.3.1.1 | Header File Length: words, itself included | `encode_header_file`, `decode_header_file` | `header_file_test`, `golden_test` |
| 2.2.3.1.2 | Load File Format Version | as 1.4.1 | as 1.4.1 |
| 2.2.3.1.3 | Part Flags: bit 0 set means a download part; the other 15 bits are 0 | `PartFlags` | `header_file_test`, `load_test` |
| 2.2.3.1.4-.11 | The eight absolute pointers; 0x0000 where a part is left out (support files, user data, load type, positions) | `encode_header_file` writes them; `decode_header_file` checks each against the body | `header_file_test` ("nothing optional", "outside the body") |
| 2.2.3.1.12, .15, .19, .23, .30, .43, .55, .56, .58 | Expansion points | skipped by pointer on reading | `header_file_test` |
| 2.2.3.1.13-.14 | Load PN: length, text, NUL to a whole word | `WordWriter::string` | `header_file_test` ("odd-length"), `golden_test` |
| 2.2.3.1.16-.18 | Load Type Description, Load Type ID; absent when the pointer is 0 | `LoadType` | `header_file_test`, `limits_test` |
| 2.2.3.1.20-.22 | Target HW IDs (count, each length and text) | `HeaderFile::target_hardware_ids` | `header_file_test`, `limits_test` |
| 2.2.3.1.24-.29 | Target HW IDs with positions, each with its positions; the ID should be in the list | `TargetHardware`, `check_header_file` | `header_file_test`, `golden_test` |
| 2.2.3.1.32-.42 | Data files: relative pointer, name, PN, length in words, CRC-16, length in bytes (64 bits), check value | `DataFile` | `header_file_test` ("length in words and in bytes"), `load_test`, `golden_test` |
| 2.2.3.1.31 | At least one data file | **error** on writing and reading | `header_file_test`, `load_test` |
| 2.2.3.1.37, .39 | A half-word at the end of a data file counts as a word; words and bytes must agree | `words_of`; disagreement is an **error** on reading | `header_file_test` |
| 2.2.3.1.44-.54 | Support files: pointer, name, PN (may be empty), length in bytes (32 bits), CRC-16, check value | `SupportFile` | `header_file_test`, `load_test`, `golden_test` |
| 2.2.3.1.40-.42, .52-.54, .59-.61 | Check value: length counting its own two fields, type, value; 0x0000 and nothing more when unused | `WordWriter::check_value`, `WordReader::check_value` | `header_file_test`, `integrity_checker_test` |
| 2.2.3.1.57 | User defined data, whole words, optional | `HeaderFile::user_defined_data` | `header_file_test`, `golden_test` |
| 2.2.3.1.61 | Load Check Value over the whole load without its own fields and the two CRCs | `build_load`, `verify_load` | `load_test`, `golden_test` |
| 2.2.3.1.62 | Header File CRC-16 over the header without itself and the Load CRC | `encode_header_file`, `decode_header_file` (**error** `bad_crc`) | `header_file_test`, `robustness_test` |
| 2.2.3.1.63 | Load CRC-32 over the whole load without itself, computed after the Header File CRC | `build_load`, `verify_load` | `load_test`, `golden_test` |
| 2.2.3.2, 2.2.3.3 | Data and support file content is the supplier's | held as opaque bytes | `load_test` |
| 2.2.4 | Compression and encryption of files; CRCs and check values are of the final form | the library never looks inside a file, so its CRCs are of whatever bytes it is given; compressing or encrypting is for the caller | n/a |
| 2.3.1, 2.3.1.1-.20 | Batch file: version, PN, comment, load-list blocks with a position, loads (header file name, load PN), CRC-16 | `BatchFile`, `encode_batch_file`, `decode_batch_file` | `batch_file_test`, `golden_test` |
| 3.1 | Media set PN: at most 15 characters, no blanks, not ending in a hyphen, no I Q Z | `check_media_set_part_number` | `part_number_test` |
| 3.2 | 1 to 255 members; X from 1 to Y | `MediaSetIdentity`; X = 0 or X > Y is an **error** | `media_files_test`, `media_set_test` |
| 3.2.1 | LOADS.LUM, FILES.LUM and BATCHES.LUM in every member's root; all files listed in FILES.LUM but itself; files within four directories; a file wholly on one member | `build_media_set`, `read_media_set` | `media_set_test` |
| 3.2.2 | Reserved extensions (Table 3.2.2-1) | `is_reserved_extension`, and the file name checks | `naming_test` |
| 3.2.3.1, .1-.25 | LOADS.LUM: PN, X and Y, per load a PN, header file name, member, target IDs; user data; CRC-16 | `LoadsList`, `encode_loads_list`, `decode_loads_list` | `media_files_test`, `golden_test` |
| 3.2.3.2, .1-.30 | FILES.LUM: per file a name, pathname, member, CRC-16, check value; user data; its own check value and CRC-16 | `FilesList`, `encode_files_list`, `decode_files_list` | `media_files_test`, `media_set_test`, `golden_test` |
| 3.2.3.2.20 | A file's CRC in FILES.LUM is of the whole file; for LOADS.LUM and BATCHES.LUM it is 0x0000 | `build_media_set` | `crc_test`, `media_files_test`, `media_set_test` |
| 3.2.3.3, .1-.22 | BATCHES.LUM: per batch a PN, file name, member; user data; CRC-16 | `BatchesList`, `encode_batches_list`, `decode_batches_list` | `media_files_test`, `golden_test` |
| 3.2.3 | The lists are the same on every member but for X and the CRC | `build_media_set` builds them from one description; `read_media_set` refuses members that disagree (**error** `inconsistent`) | `media_set_test` |
| 3.2.4.1.1 | A load's files are under its part root directory, named for the PN without hyphens | `PlacedLoad::part_root_directory` | `media_set_test` |
| 3.2.4.1.1-.2 | ARINC 615 and Boeing legacy parts: `diskNNN` directories under the part root | `FilePlacement::subdirectory` | `media_set_test`, `compatibility_test` |
| 3.2.4.2 | Resolving a file name: within the part root directory, then by CRC, then the first | `resolve_listed` in `media_set.cpp` | `media_set_test` (duplicate names), `compatibility_test` |
| 3.2.4.3 | Directory structure for electronic distribution | not built (see 1.6) | n/a |
| 3.3 | Label: the seven required items, the recommended identification, the date format | `MediaLabel`, `format_label`, `check_label` | `media_label_test` |
| 3.3.2 | Label layout, type size, colour | for whatever prints the label | n/a |
| 3.4 | Media types: FAT disks, PC Cards, ISO 9660 CDs, hard disks | the file system is outside the library | n/a |
| 4.1-4.2 | CRC definition and rules: most significant bit first, no reflection | `Crc<>`, `make_crc_table` | `crc_test` |
| 4.3 | Parameters: CRC-8 (X^8+1, init 0), CRC-16 (0x1021, init 0xFFFF), CRC-32 (0x04C11DB7, init and xor-out 0xFFFFFFFF); the check values for `00 01 ... FF` | `crc8_parameters`, `crc16_parameters`, `crc32_parameters` | `crc_test` (also as `static_assert`) |
| 4.4.1 | A CRC stored in its own domain leaves its field out | each file's CRC covers all before it; the Load CRC leaves out its own two words | `crc_test`, `load_test`, `robustness_test` |
| 4.4.2 | CRC-16 for small files, CRC-32 up to 512 MB: split larger files | advice to the supplier; not enforced | n/a |
| 5.1 | Check values stored big-endian in whole words; an odd-sized value preceded by a zero byte | `IntegrityChecker::check_value`, `crc_bytes` | `integrity_checker_test` |
| 5.2, Table 5.1 | Types 1 to 5 and their lengths: CRC-8 (2 bytes), CRC-16 (2), CRC-32 (4), MD5 (16), SHA-1 (20) | `CheckValueType`, `check_value_length`, `IntegrityChecker` | `check_value_type_test`, `integrity_checker_test` |
| 5.3.1 | MD5 as RFC 1321 | `Md5` | `digest_test` (RFC test suite, a million `a`) |
| 5.3.2 | SHA-1 | `Sha1` | `digest_test` (FIPS 180 vectors, a million `a`) |
| App. A, B | Structure of a load and of a media set | the data model | `compatibility_test` |
| App. C | Header file, LOADS.LUM and FILES.LUM layouts | as above, with the exceptions below | `golden_test` |
| App. D | Reserved | n/a | n/a |
| App. F | Loads that ARINC 615 and Boeing 777 loaders can load too: CONFIG.LDR and EXCONFIG.LDR as support files; the legacy header as a support file; DISK.DIR and NON_LOAD.CRC in FILES.LUM only | `arinc615::ConfigLdrFile`, the file name checks, `PlacedFile` | `compatibility_test` |
| App. K, L | The CRC program and the reference files and their CRCs | `Crc<>` | `crc_test` (see [Not yet verified](#not-yet-verified)) |

## Places the standard is unclear

These are the choices the library had to make because the text, a table and a
figure do not say the same thing, or do not say enough. Each is made in one
place, so changing it is a small change; each deserves a check against a load
from another tool.

1. **What order the Load CRC and Load Check Value take the files in.**
   Sections 2.2.3.1.61 and 2.2.3.1.63 say "the entire load" and list the data
   files, the support files and the header, but give no order. The library
   takes the header file first, then the data files, then the support files, each
   in the order the header lists them (`load.cpp`, `load_check_value`/`load_crc`).
   The Load CRC covers the header but for its last two words (the Load CRC); the
   Load Check Value covers the header up to its own fields, so it leaves out the
   Header File CRC and the Load CRC too. **Confirmed by the project owner as the
   intended order** (header, data files, support files). That is a decision about
   what to build, not a check against another tool's files, so this remains the
   most likely place to disagree with one.
2. **The support file entry.** Table 2.2.3-1 and the field descriptions give a
   support file a 32-bit length in bytes, a CRC-16 and a check value. Figure C-1
   shows, for support files, a 32-bit length followed by the CRC and a *64-bit*
   length in bytes, as for data files. The library follows the table and the text
   (`header_file.cpp`, `write_support_files`).
3. **The lists in Figures C-4 and C-5.** The figures show a "Header File
   Pathname" in LOADS.LUM and no File Name in FILES.LUM. Tables 3.2.3.1 and
   3.2.3.2 and the field descriptions (3.2.3.1.16-.17, 3.2.3.2.15-.18) give
   LOADS.LUM a header file *name* and FILES.LUM both a name and a pathname. The
   library follows the tables and the text.
4. **The pointer to the next load-list block in a batch file** (2.3.1.12) is
   "the pointer to the first word in the group of data for the next block". The
   library writes it relative, like the other list pointers, so the distance from
   the pointer to the next block's pointer.
5. **No check value.** Section 5.1 says the length and type of an absent check
   value are set to values that are garbled in the copy this was written from.
   Sections 2.2.3.1.40-.42 and the matching ones for the other files say the
   length is 0x0000 and the type and value are left out. The library does that.
6. **FILES.LUM's own check value** (3.2.3.2.27-.29) is said to be a check value of
   the file; not what it covers. The library takes the file up to the check value
   fields, as the Load Check Value does.
7. **File names** (2.2.2). The list of characters not allowed reads
   `"~", "/", ":", "\", "I", "O", "Q"` and a blank in this copy. I, O and Q cannot
   be meant for file names (the standard's own `CONFIG.LDR` has an I and an O), so
   the library checks for `~ / : \` and blanks and non-printable characters only.
8. **Four directory levels** (3.2.1). Read as at most four directories below the
   root. Read as including the root it would be three, which would draw findings on
   sets that are within the standard.
9. **LOADS.LUM and BATCHES.LUM are listed once for each member** in FILES.LUM,
   since they differ from member to member and each entry names its member.
10. **Data File Length in Bytes** is written for every data file, as Figure C-1
    shows it; Table 2.2.3-1's notes are garbled where they say whether it is
    optional.
11. **Versions.** The decoders accept the right class at the version in 1.4.1 or
    later. A load header of version 0x8003 has no pointer to the Load Check Value
    and is refused (`bad_version`) rather than guessed at.

## What reading is lenient about

The standard says some things "should" and a loader is expected to cope. The
decoders do not refuse: spare Part Flags bits that are not 0; the value of the
NUL that pads an odd-length text; a part number, file name or target hardware ID
that is not in the recommended form (these are findings); a Load Check Value or
file check value of a type the library cannot calculate (it is read and kept, and
not checked).

## Not yet verified

- **Another tool's files.** The second implementation, `make_golden.py`, was
  written from the same text, so a shared misreading would not show. The
  [unclear places](#places-the-standard-is-unclear) above are where that is most
  likely. A load and a media set written by an established tool, read here, and
  written here, read there, would settle it.
- **The random reference files of Appendix L** (T05 to T09) and the Ethernet
  frame (T10) are posted by the publisher and are not in the standard, so their
  CRCs are not tested. The files the appendix describes exactly (T01-T04, T11-T13)
  are, for every CRC width the appendix gives a result for.
- **Manufacturer's codes** are checked for form, not against Attachment 1.
- **Reading a media set from a disk**, as opposed to from `MediaMember`s in
  memory, does not exist yet. It belongs with the platform section and the 615A
  work (docs/roadmap.md).
- **Linux and macOS** have not run the tests (docs/building.md).
