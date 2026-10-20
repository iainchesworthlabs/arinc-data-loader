**`adl::arinc615a`, the codec of ARINC 615A-3** (`src/arinc615a`,
[docs/arinc615a-codec.md](docs/arinc615a-codec.md)): TFTP packets (RRQ, WRQ, DATA,
ACK, ERROR, OACK) with the options of RFCs 2347 to 2349 and the `checksum_<n>` and
`port` options of 615A, block numbers that wrap from 65535 to 1, and the WAIT and
ABORT messages; the FIND request and answer; and the twelve protocol files of
section 6.4 with the status code table. Encoders write into a buffer the caller
supplies and decoders return views over the bytes they were given, with every
length checked, so nothing allocates, throws or needs RTTI. The places where the
standard is unclear, and what was chosen, are listed in the page above. Tested
against the standard's own numbers, round trips, every truncation and bit change,
random bytes, and byte for byte against an independent writer
(`tests/golden/make_golden_615a.py`).
