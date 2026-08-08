# Format and Confidence Notes

This document records the fields currently exposed by NVIDIA BIOS Reader and
the confidence assigned to their interpretation.

## Confidence levels

| Level | Meaning |
| --- | --- |
| Documented | Structure or field is described by public NVIDIA documentation |
| Cross-validated | Repeated across real ROMs and checked against independent tools |
| Observed | Consistent in the samples examined but not independently specified |
| Inferred | Useful interpretation derived from surrounding data or behavior |
| Unknown | Preserved as a raw value without a semantic claim |

## Parsing path

The parser begins with the PCI expansion-ROM headers and selects the NVIDIA
legacy image. It then locates the `BIT` table and uses its tokens rather than
searching for isolated byte patterns.

| Data | Source | Confidence |
| --- | --- | --- |
| PCI Vendor and Device IDs | Legacy image `PCIR` structure | Documented |
| VBIOS version | BIT information token | Cross-validated |
| GPU chip name | Family/die code lookup | Cross-validated for listed chips |
| Memory Information pointer | BIT memory token | Documented structure, cross-validated use |
| Memory descriptor bit fields | First dword of each Memory Information record | Cross-validated/observed |
| Physical strap mapping | Memory strap translation table | Cross-validated |
| RAMCFG L/M/H codebook | NVIDIA board schematics and repeated board layouts | Cross-validated for the listed 0-15 codes |
| Timing map and record pointers | BIT performance token | Documented structure, cross-validated use |
| Complete timing-record bytes and CRC32 | Declared timing-table geometry | Cross-validated raw extraction |
| Post-legacy pointer adjustment | PCI image layout plus observed logical offsets | Observed/cross-validated |
| NVIDIA/Afterburner MCLK range | Timing-map low/high values | Observed/cross-validated |
| Memory-device clock | MCLK divided by four for GDDR6 or eight for GDDR6X | Inferred/cross-validated against GPU-Z |
| CONFIG0 through CONFIG5 names | Reverse-engineered field mapping | Observed |

## Memory descriptor

For the currently supported layout, the first little-endian dword of a Memory
Information record is separated as follows:

| Bits | Current interpretation |
| --- | --- |
| 0-3 | Memory type code |
| 4-7 | Strap selector code |
| 8-11 | Variant index |
| 12-15 | Memory vendor code |
| 16-19 | Revision code |
| 20-23 | Per-device density code |
| 24-26 | Organization code |
| 27-31 | Feature/unknown code |

The descriptor's logical entry number is also used as a timing-map group index
in the validated ROMs. The translation table maps physical strap values back
to those logical entries.

## Physical RAMCFG codebook

NVIDIA boards can strap each of three selector inputs to low (`L`), midpoint
(`M`), or high (`H`) voltage. Although three three-level inputs allow 27
electrical combinations, the observed RAMCFG convention defines the following
16-code subset. It is a codebook, not ordinary base-three counting.

| Code | RAMCFG[4:0] | STRAP2 | STRAP1 | STRAP0 |
| ---: | :---: | :---: | :---: | :---: |
| 0 | `00000` | L | L | L |
| 1 | `00001` | L | L | H |
| 2 | `00010` | L | H | L |
| 3 | `00011` | L | H | H |
| 4 | `00100` | H | L | L |
| 5 | `00101` | H | L | H |
| 6 | `00110` | H | H | L |
| 7 | `00111` | H | H | H |
| 8 | `01000` | L | L | M |
| 9 | `01001` | L | M | L |
| 10 | `01010` | L | M | H |
| 11 | `01011` | L | H | M |
| 12 | `01100` | M | L | L |
| 13 | `01101` | M | L | H |
| 14 | `01110` | M | H | L |
| 15 | `01111` | M | H | H |

This electrical codebook and the VBIOS translation table are different
layers. A code can exist electrically while being absent from a particular
ROM's declared translation-table count. The reader reports only declared
translation bytes as selectable mappings; it does not interpret adjacent ROM
bytes as additional entries.

The GUI therefore reports three separate counts: all records declared by the
Memory Information table, non-`Skip` descriptors shown in the grid, and unique
descriptors referenced by at least one declared physical translation code. A
large record count does not by itself mean that every record is selectable or
timing-complete. The compact grid label `Unmapped` means precisely "not
referenced by the declared translation table"; the detailed view and report
retain the longer wording.

The RAMCFG map always displays the 16 standard codebook values while keeping
the translation table's declared count authoritative. Codes beyond that count
are labeled `outside declared table` and are never read from adjacent bytes.
Each declared mapping reports the exact translation-byte offset. When two
physical codes contain the same target byte, later occurrences are labeled as
aliases of the first declared code.

Alias currently means only "same translation target in this ROM." It does not
yet prove that every firmware script ignores the original physical code. The
active physical RAMCFG cannot be recovered from a saved ROM file alone.

Density is per memory device. Neither density nor organization alone proves
the total memory capacity or physical population of a board. In particular,
an `x8` or clamshell-capable profile in the ROM may be an unused alternative.

## Clock domains

The timing-map low/high values correlate directly with the MCLK domain exposed
by NVIDIA telemetry and MSI Afterburner. Different tools present the same
physical memory using different clock conventions:

| Memory type | NVIDIA/Afterburner MCLK | Device clock shown by GPU-Z | Effective data rate |
| --- | ---: | ---: | ---: |
| GDDR6 example | 7000 MHz | 1750 MHz (`MCLK / 4`) | 14000 Mb/s |
| GDDR6X example | 11501 MHz | about 1437.6 MHz (`MCLK / 8`) | about 23002 Mb/s |

The reader displays the firmware MCLK range directly and labels the converted
device clock as inferred. Neither value is a P-state name, and a wide final
range such as `8500-16383` is a selector interval rather than the claimed
operating clock of the card.

## Timing coverage

Each used frequency range contains one timing ID for every logical memory
group. `FF` means that the range has no timing-record reference for that group.
The reader classifies a profile as follows:

| Status | Rule |
| --- | --- |
| `FULL` | Every used range references a present, non-zero record |
| `PARTIAL` | At least one used range is present and at least one is `FF` |
| `EMPTY (all FF)` | Every used range is `FF` |
| `INVALID REFERENCE` | A reference is missing, outside the table, or all zero |
| `UNAVAILABLE` | A compatible timing map/table was not located |

A `0-0` range is treated as an unused slot and does not affect coverage.

## Decoded timing fields

The current decoder extracts the following fields from the first 24 bytes of a
timing record:

`RC`, `RFC`, `RAS`, `RP`, `CL`, `WL`, `RD_RCD`, `WR_RCD`, `RPRE`, `WPRE`,
`CDLR`, `WR`, `W2R_BUS`, `R2W_BUS`, `FAW`, `REFRESH`, `RRD`, and `WRCRC`.

These are raw memory-controller values or cycle counts. Comparing two profiles
at the same clock can be useful, but smaller is not universally "better": some
fields encode delays differently, training behavior also matters, and not all
record bytes have been decoded.

## Complete timing records

The timing-table header declares the complete record stride. In the validated
GA104 sample documented for v0.4.0-beta.1, each record is 76 bytes. The reader
preserves every byte even though only offsets `+0x00` through `+0x17` currently
have named CONFIG0..CONFIG5 fields.

CRC32 fingerprints are used to group byte-identical records and make
cross-report comparisons convenient. CRC32 is not a cryptographic identity;
ROM samples must still be identified by SHA-256.

The profile comparator distinguishes:

- decoded fields equal and complete records equal;
- decoded fields equal but complete records different;
- decoded fields and complete records different;
- records that cannot be compared because a map entry is `FF` or invalid.

Bytes after `+0x17` remain explicitly unknown. A repeating correlation such as
the observed `+0x33 A2/A3` difference between one GA104 Samsung 8/16 Gbit pair
is evidence for further study, not enough evidence to assign a field name.

## Pointer address domains

BIT table pointers are logical offsets associated with the NVIDIA legacy
image. A physical ROM file can contain another PCI image between the legacy
image and later data tables. Consequently, a raw pointer printed by firmware
tools is not always the same as a file offset.

The currently observed resolution rule is:

```text
resolved = legacy image base + raw pointer
```

For a logical pointer beyond the declared legacy-image length, when an
immediately following UEFI PCI image is present:

```text
resolved = legacy image base + raw pointer + intervening UEFI image length
```

The reader reports the raw pointer, the byte that stores it, the resolved file
offset, whether the intervening image adjustment was applied, and a confidence
label. This rule is bounds-checked and validated on the current corpus, but
unusual multi-image layouts should be treated as experimental.

See the [GA104 8/16 Gbit case study](research/ga104-8gbit-vs-16gbit.md)
for a worked example.

## Public references

- [NVIDIA BIOS Information Table Specification](https://download.nvidia.com/open-gpu-doc/BIOS-Information-Table/1/BIOS-Information-Table.html)
- [NVIDIA Memory Clock Table Specification](https://download.nvidia.com/open-gpu-doc/MemoryClockTable/1/MemoryClockTable.html)
- [NVIDIA open-gpu-doc Virtual BIOS repository](https://github.com/NVIDIA/open-gpu-doc/tree/master/virtual-bios)

The public documents do not define every generation-specific memory descriptor
or every timing-record bit. Those gaps are the reason this project preserves
raw values and explicit confidence labels.
