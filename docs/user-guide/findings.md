# Findings of the media set check

A *finding* is a place where a load or a media set departs from what ARINC 665-3 recommends. The standard says "must" for some things and "should" for others. Where it says "must", the file is refused. Where it says "should", the file is read, and the departure is reported as a finding. A finding is a warning: it does not stop the loader from reading a set or from uploading it. *Finding* is this project's own word.

You see findings in three places:

- In the loader, as the warnings of a media set: the count in the check line (`; %n warning(s) against the standard's recommendations`) and the list in **Properties...** ([Choose and check a media set](choose-a-media-set.md)).
- In the generator, as marks on the fields and the rows of the form ([Make a media set](make-a-media-set.md)).
- In the output of `adl_write_payloads`, as an indented line after each scenario ([Command line and exit codes](command-line.md#an-example)).

Each finding has the number of the section of ARINC 665-3 that it is about, and a sentence. A value that the program fills in is shown here as `…`. The sentences are in English in every language. This page lists every one that the library `adl::arinc665` can produce when it checks a load, a media set, a name or a part number, with the section the project took it from; the standard's own words are not copied here.

What to do about a finding: if you want the set to meet the recommendation, change the file with the tool that made it, or the field in the generator, and check the set again. If you meant it, as the sample sets that follow the standard's own examples do, leave it.

The library also checks a media label (section 3.3 of the standard). None of the programs reads a label, so those messages come only from a program of your own that calls `adl::arinc665::check_label`.

## The messages

### Part numbers

| Clause | Message |
| --- | --- |
| 2.1.1 | `the part number has an embedded blank` |
| 2.1.1 | `the part number is not in the form MMMCC-SSSS-SSSS` |
| 2.1.1 | `the manufacturer's code is not three upper-case alphanumeric characters` |
| 2.1.1 | `the product identifier is not SSSS-SSSS of upper-case alphanumerics other than I, O, Q and Z` |
| 2.1.3 | `the check characters are not two upper-case hexadecimal digits` |
| 2.1.3 | `the check characters are not the ones worked out from the rest of the part number` |
| 3.1 | `the media set part number is empty` |
| 3.1 | `the media set part number is longer than 15 characters` |
| 3.1 | `the media set part number has an embedded blank` |
| 3.1 | `the media set part number ends in a hyphen` |
| 3.1 | `the media set part number uses I, Q or Z, which the standard recommends against` |

### File names, pathnames and target hardware IDs

| Clause | Message |
| --- | --- |
| 3.2.2 | `…the extension … of a … is reserved for another use` |
| 2.2.2 | `the file name is empty` |
| 2.2.2 | `the file name is longer than 255 characters` |
| 2.2.2 | `"." and ".." are not allowed as file names` |
| 2.2.2 | `the file name has a blank or a non-printable character` |
| 2.2.2 | `the file name has one of the characters ~ / : \` |
| 2.2.2.1 | `the header file name does not end in .LUH` |
| 2.2.2 | `the header file name does not begin with a manufacturer's code` |
| 2.3.1 | `the batch file name does not end in .LUB` |
| 3.2.3.2.18 | `the pathname does not start at the root with a backslash` |
| 3.2.3.2.18 | `the pathname does not end with a backslash` |
| 3.2.1 | `the file is more than four directories deep` |
| 3.2.3.2.18 | `the pathname has an empty directory name` |
| 1.5 | `a target hardware ID is 4 to 15 characters long` |
| 1.5 | `a target hardware ID is four hexadecimal characters or begins with a manufacturer's code` |

### A load (the header file)

| Clause | Message |
| --- | --- |
| 1.4.1 | `the load file format version is not 0x8004` |
| 2.2.3.1.20 | `the load lists no target hardware ID` |
| 2.2.3.1.26 | `target hardware ID … with positions is not in the target hardware ID list` |
| 2.2.1 | `the file name … is used more than once in the load` |
| (varies) | `…the … part number … is used more than once in the load` |
| 2.2.3.2 | `the data file … is not a whole number of 16-bit words` |

### A batch file

| Clause | Message |
| --- | --- |
| 1.4.1 | `the batch file format version is not 0x9004` |

### The lists of a media set (LOADS.LUM, FILES.LUM, BATCHES.LUM) and the set

| Clause | Message |
| --- | --- |
| 3.2.3.1.10 | `the media sequence number is not between 1 and the number of members` |
| 1.4.1 | `the media file format version is not 0xA004` |
| 3.2.3.1.18 | `the load … is on a member the set does not have` |
| 3.2.3.1.12 | `the load … is listed more than once` |
| 3.2.3.2.19 | `the file … is on a member the set does not have` |
| 3.2.3.2.13 | `FILES.LUM lists itself` |
| 3.2.3.3.18 | `the batch … is on a member the set does not have` |
| 3.2.1 | `the file … on member … is not listed in FILES.LUM` |

### The media label

| Clause | Message |
| --- | --- |
| 3.3.1 | `the label has no media set nomenclature` |
| 3.3.1 | `the media sequence number is not between 1 and the number of members` |
| 3.3.1 | `the label lists no software part number (or refers to LOADS.LUM)` |
| 3.3.1 | `the label has no supplier identification` |
| 3.3.1 | `the label has no media set serial number` |
| 3.3.1 | `the label has no product acceptance or release stamp` |
| 3.3.1 | `the media creation date is not a valid day, month and year` |

A finding whose clause is shown as (varies) takes its section from the part that is being checked.
