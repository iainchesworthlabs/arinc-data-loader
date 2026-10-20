**`adl::arinc665`, the whole of ARINC 665-3's file formats.** Loads (header,
data and support files), batch files, and media sets with `LOADS.LUM`,
`FILES.LUM` and `BATCHES.LUM`, each written and read with its lengths, pointers
and CRCs checked; media labels; part numbers and their check characters. A load
is built from file contents and verified end to end, and a media set is built
from loads and read back by the rules of section 3.2.4. Departures from the
standard's recommendations are reported as findings rather than refused. See
[docs/arinc665-coverage.md](docs/arinc665-coverage.md).
