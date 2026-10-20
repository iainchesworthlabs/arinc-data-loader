**Tests against a second implementation.** `tests/golden/make_golden.py` writes
each file format separately from the standard's tables, and the tests compare
the library's output with it byte for byte; payloads for the ARINC 615 and
Boeing 777 layouts of Appendix F, for the limits of the field widths, and for
damaged files.
