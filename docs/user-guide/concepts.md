# What ARINC 615A and ARINC 665 are

Two standards from the aviation world cover loading software into aircraft equipment. This
software follows both. You do not need to read them to use these programs, but you will meet their
words, so here they are in plain terms.

## ARINC 665: how a load is packaged

ARINC 665 describes the *package*: what a loadable piece of software looks like on disk or on a
medium that travels with it. A **load** is one loadable thing. It consists of a **header file**
that describes the load (its part number, which kinds of equipment it is for, and a list of its
other files with a check value for each) and one or more **data files** (the payload) and
optional **support files** (configuration and the like). Everything is protected by **check
values**, small numbers calculated from a file's contents (a CRC or a digest such as MD5 or
SHA-1): if one byte changes, the check value no longer matches and the load is refused. One or
more loads travel together on a **media set**, which is made of **members**, the individual disks
(or, here, folders) of the set, each with three list files (`LOADS.LUM`, `FILES.LUM`,
`BATCHES.LUM`) that say what is on that member. A **batch file** can name loads for several
targets so that one operation loads a whole aircraft's worth. Parts are identified by **part
numbers** with strict shapes, which is why the generator checks them as you type
([the rules](make-a-media-set.md#the-rules-the-generator-enforces)).

## ARINC 615A: how a load gets into the equipment

ARINC 615A describes the *conversation* between a **data loader** (this program, the thing the
operator uses) and a **target** (the equipment being loaded) over a network. The loader and the
target exchange small files by a simple file-transfer protocol (TFTP, here over UDP): the loader
writes a request file, the target answers with **status files** that say how it is getting on, and
files move in whichever direction the job needs. There are three **operations**:

- **Information**: ask the target what it is and what it holds (its hardware and the software
  parts installed in it).
- **Upload**: send one or more loads from the loader to the target. This is what the standard
  calls loading; the program says Upload.
- **Download**: fetch files from the target, for example logs or maintenance data. In the *media
  defined* mode the operator names the files; in the *operator defined* mode the target lists what
  it can give and the operator picks.

A loader can also look for targets: **FIND** broadcasts a question to the network and every target
that hears it answers with its identity (hardware identifier, type name, position, literal name,
manufacturer code).

## Words this guide uses

| Word | Meaning |
| --- | --- |
| Media set | The folder tree that holds one or more loads: one folder per member, each with its three list files |
| Member | One disk of a media set; here a folder called `member_001`, `member_002` and so on |
| Load | One loadable thing: a header file plus its data and support files |
| Part number | The identity of a load, a batch or a media set. A load's looks like `ACM47-1234-5678` |
| Check value | A number calculated from a file's contents, used to prove it arrived unchanged |
| Finding | Something in a media set that differs from what the standard recommends, but does not stop it being read; the loader shows these as warnings |
| Target | The equipment being loaded (or the mock target that stands in for it) |
| `THW_ID_POS` | How a target is addressed in file names: its hardware identifier, an underscore and its position, for example `HNPFMS_L` for hardware `HNPFMS` at position `L`. A target with no position is just its identifier |
| Control port | The UDP port the target's file server listens on: 59 in the standard. The loader and the target can share one machine if one of them uses another port |
| FIND port | The UDP port FIND uses: 1001 |
| Activity log | The loader's list of what the target said and what the loader noticed, with the time |
| Receipt | The summary the loader shows after a transfer |

## What these programs are, and are not

The generator and the loader sit on the **source side**: they run on the operator's computer. The
mock target is a **stand-in** for a target, meant for demonstrations and tests. There is not yet a
program that runs *on* real equipment. See [the limits](files-and-limits.md#what-has-not-been-checked)
for what has and has not been checked.

Next: [installing and starting the programs](installing.md).
