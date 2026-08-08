# Manual Memory-Table Inspection

This procedure mirrors the current parser and can be performed with a hex
editor such as [ImHex](https://imhex.werwolv.net/). It is intended for Turing,
Ampere, and related ROM layouts that use a compatible NVIDIA BIT table.

Work on a copy of the ROM. All multibyte integers below are little-endian, and
all offsets are hexadecimal unless stated otherwise.

## 1. Locate the NVIDIA BIT table

Search for these six bytes:

```text
FF B8 42 49 54 00
```

`42 49 54` is the ASCII text `BIT`. At the start of this signature:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| `+08` | 1 | BIT header length |
| `+09` | 1 | Token-record length |
| `+0A` | 1 | Token count |

Token records begin at `BIT + header length`. Each record begins with an ASCII
token ID and, in the layouts currently handled by the reader, contains:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| `+00` | 1 | Token ID |
| `+01` | 1 | Token version |
| `+02` | 2 | Token data length |
| `+04` | 2 | Pointer to token data |

The pointer is relative to the start of the legacy PCI ROM image, which is
usually offset zero in a standalone VBIOS dump.

## 2. Find the Memory token

Walk the token records until the first byte is `4D`, ASCII `M`. The current
reader expects version 2 and at least five bytes of token data.

Follow the token pointer. Its data begins with:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| `+00` | 1 | Number of logical memory groups |
| `+01` | 2 | Pointer to physical-strap translation table |
| `+03` | 2 | Pointer to Memory Information table |

These two pointers are also relative to the legacy image base.

## 3. Decode Memory Information entries

At the Memory Information pointer, read the four-byte table header:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| `+00` | 1 | Table version |
| `+01` | 1 | Header length |
| `+02` | 1 | Record length |
| `+03` | 1 | Record count |

The first record begins at `table + header length`. Subsequent records are
separated by `record length` bytes. Interpret the first four bytes of each
record as one little-endian 32-bit descriptor:

| Bits | Current interpretation |
| --- | --- |
| 0-3 | Memory type |
| 4-7 | Strap selector code |
| 8-11 | Variant index |
| 12-15 | Vendor code |
| 16-19 | Revision code |
| 20-23 | Per-device density code |
| 24-26 | Organization code |
| 27-31 | Feature/unknown code |

Frequently observed codes include:

| Field | Code | Label |
| --- | --- | --- |
| Memory type | `3` | GDDR5 |
| Memory type | `9` | GDDR6 |
| Memory type | `A` | GDDR6X |
| Vendor | `1` | Samsung |
| Vendor | `6` | Hynix |
| Vendor | `F` | Micron |
| Density | `5` | 8 Gbit per device |
| Density | `6` | 16 Gbit per device |

Organization labels remain generation- and memory-type-dependent. Preserve the
raw code when the physical topology has not been independently verified.

## 4. Map physical straps to entries

At the translation-table pointer, read one byte for each logical memory group.
The byte position is the physical strap value; the byte value is the zero-based
Memory Information entry selected by that strap.

Example:

```text
00 01 02 03 04 05 06 00
```

Here physical straps 0 and 7 both select logical entry 0, while physical strap
1 selects entry 1, and so on.

This table shows which profiles the firmware can select. It does not identify
the active hardware strap in a saved ROM file.

For the multilevel RAMCFG convention seen on the validated Turing and Ampere
boards, decode physical values `0` through `15` as follows:

```text
 0 L/L/L   1 L/L/H   2 L/H/L   3 L/H/H
 4 H/L/L   5 H/L/H   6 H/H/L   7 H/H/H
 8 L/L/M   9 L/M/L  10 L/M/H  11 L/H/M
12 M/L/L  13 M/L/H  14 M/H/L  15 M/H/H
```

Keep the table length from the Memory token authoritative. For example, if it
declares 14 entries, physical codes 0 through 13 are mapped; bytes immediately
following them are not codes 14 and 15 merely because they contain plausible
values.

If two declared bytes contain the same logical target, describe the later
physical code as a **translation alias** of the first. This proves only that
the Memory Information/timing-map lookup converges on the same group. It does
not prove that initialization scripts, runtime firmware, or electrical
training ignore the original physical code. The reader exposes this procedure
directly through the GUI **RAMCFG Map** button and the CLI `--ramcfg` option.

## 5. Locate the timing map and timing records

Return to the BIT tokens and find token `P` (`50` in ASCII). Follow its token
pointer to the performance data. In the currently validated layout:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| `+04` | 4 | Timing-map pointer |
| `+08` | 4 | Timing-record-table pointer |

These 32-bit pointers require extra care in some combined legacy/UEFI ROMs.
When a raw pointer extends beyond the legacy image, the physical file offset
may be shifted by the intervening UEFI image length. Confirm the table headers
before treating a calculated offset as valid.

For the validated GA104 sample, the practical calculation is:

```text
small pointer: file = legacy base + raw pointer
large pointer: file = legacy base + raw pointer + intervening UEFI length
```

Do not assume that the legacy image begins at zero or that the UEFI image has a
fixed size. Read both PCI image headers. The CLI pointer map prints the source
byte, raw pointer, resolved offset, and whether an adjustment was applied.

The compatible timing map begins with:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| `+00` | 1 | Version, currently expected as `11` |
| `+01` | 1 | Header length |
| `+02` | 1 | Base-record length |
| `+03` | 1 | Extended-record length |
| `+04` | 1 | Extended-record count / memory groups |
| `+05` | 1 | Frequency-range record count |

Calculate one range-record stride as:

```text
base length + (extended length * extended count)
```

The first four bytes of each range record are the raw low and high limits as
two little-endian 16-bit integers. The timing ID for logical memory group `g`
is located at:

```text
range + base length + (g * extended length)
```

`FF` means that group has no timing record for that range. A `0-0` range is an
unused slot and should not affect coverage classification.

On the validated GDDR6/GDDR6X ROMs, these low/high limits correlate with the
high MCLK domain reported by NVIDIA tools, MODS/NVMT, and MSI Afterburner. For
comparison with the lower memory-device clock commonly shown by GPU-Z, the
current reader infers `MCLK / 4` for GDDR6 and `MCLK / 8` for GDDR6X. Preserve
the original range values when inspecting manually because these conversions
are observational rather than part of the public table specification.

## 6. Resolve one timing ID

The timing-record table uses the same six-byte geometry header. Calculate its
record stride as:

```text
base length + (extended length * extended count)
```

Timing ID `n` begins at:

```text
timing table + header length + (n * record stride)
```

Verify that `n` is lower than the declared record count and that the complete
record remains inside the ROM. The current reader decodes selected bit fields
from the first 24 bytes as CONFIG0 through CONFIG5, while preserving the exact
record offset for manual comparison. In the GA104 sample, the declared stride
is 76 bytes; always use the stride declared by the ROM rather than hard-coding
76 for another generation.

## 7. Decide whether a profile is full

For one logical memory entry, read its timing ID in every used frequency range:

- no `FF` references and every record is present: `FULL`;
- a mixture of valid references and `FF`: `PARTIAL`;
- all used ranges are `FF`: `EMPTY (all FF)`;
- an out-of-range ID or all-zero target record: invalid reference.

This identifies table completeness, not runtime stability. Memory training,
voltage, P-state transitions, and physical memory characteristics remain
separate questions.

## 8. Compare complete records

Two profiles are not equivalent merely because their decoded CONFIG0..CONFIG5
text matches. Select the same used clock range in both profiles, resolve both
timing IDs, and compare the complete `record stride` bytes.

Record differences should be written relative to each record start:

```text
+0x2C: 0A -> 4A
+0x2D: 04 -> 03
+0x2E: 00 -> 90
+0x33: A2 -> A3
```

This notation remains useful when the two records reside at different file
offsets or use different timing IDs. Do not name an unknown field from one
correlation. Record the vendor, density, organization, VBIOS hash, range,
timing IDs, record offsets, and all changed bytes so another contributor can
test the hypothesis independently.

The CLI automates the same procedure:

```text
nvidia-bios-reader-cli card.rom --compare-profiles 1 7
```
