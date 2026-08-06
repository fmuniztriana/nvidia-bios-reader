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
| Timing map and record pointers | BIT performance token | Documented structure, cross-validated use |
| Displayed memory clock | Raw range divided by four | Inferred |
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

Density is per memory device. Neither density nor organization alone proves
the total memory capacity or physical population of a board. In particular,
an `x8` or clamshell-capable profile in the ROM may be an unused alternative.

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

## Public references

- [NVIDIA BIOS Information Table Specification](https://download.nvidia.com/open-gpu-doc/BIOS-Information-Table/1/BIOS-Information-Table.html)
- [NVIDIA Memory Clock Table Specification](https://download.nvidia.com/open-gpu-doc/MemoryClockTable/1/MemoryClockTable.html)
- [NVIDIA open-gpu-doc Virtual BIOS repository](https://github.com/NVIDIA/open-gpu-doc/tree/master/virtual-bios)

The public documents do not define every generation-specific memory descriptor
or every timing-record bit. Those gaps are the reason this project preserves
raw values and explicit confidence labels.
