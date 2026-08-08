# GA104 Samsung 8 Gbit versus 16 Gbit profile study

Status: **observed, experimental, and reproducible on the identified sample**.
This note records evidence without claiming that the same byte semantics apply
to every NVIDIA generation or VBIOS.

## Research question

Does a timing profile classified as `FULL` contain all information needed to
initialize a different memory density, and are profiles with equal decoded
CONFIG0..CONFIG5 fields actually byte-identical?

## Sample identity

| Property | Value |
| --- | --- |
| Board family | Palit GeForce RTX 3070 |
| GPU | GA104 |
| PCI Device ID | `0x2484` |
| VBIOS | `94.04.3A.40.85` |
| File size | 999,424 bytes |
| SHA-256 | `EA1583BD156BCCE43877606734307E892CD333D6329349097BBFCA1C1A1D0112` |
| Installed devices during capture | Samsung 8 Gbit GDDR6 |
| Working physical selector | RAMCFG 0 / L-L-L |
| Experimental selector | RAMCFG 6 / H-H-L |

The ROM is not redistributed by this repository. Runtime evidence was
collected independently with a locally held MODS/NVMT environment.

## Pointer resolution

The NVIDIA legacy image begins at file offset `0x9200` and has length
`0xFE00`. A `0x16600`-byte UEFI image follows it. Small pointers within the
legacy image resolve as:

```text
file offset = legacy image base + raw pointer
```

For the observed logical pointers beyond the legacy image, the inserted UEFI
image must also be included:

```text
file offset = legacy image base + raw pointer + intervening UEFI image length
```

Examples:

```text
Memory Information:
  0x9200 + 0x4126 = 0xD326

Memory Clock / timing map:
  0x9200 + 0x683BA + 0x16600 = 0x87BBA

Memory Tweak / timing records:
  0x9200 + 0x69F40 + 0x16600 = 0x89740
```

This is why raw BIT/NVMT pointers and physical file offsets must not be mixed.
The reader reports both and explicitly states when the intervening image was
included. More ROM layouts must be validated before treating the observed
threshold rule as universal.

## Static table map

| Structure | File offset |
| --- | ---: |
| Legacy image | `0x9200` |
| BIT table | `0x93B0` |
| BIT M token | `0x94B7` |
| Strap translation | `0xD238` |
| Memory Information | `0xD326` |
| Timing map | `0x87BBA` |
| Timing table | `0x89740` |
| Timing record size | 76 bytes |
| Timing record count | 65 |

## Profile comparison

Entry 1 is Samsung GDDR6 8 Gbit and entry 7 is Samsung GDDR6 16 Gbit. Both
have `FULL` timing-map coverage. Entry 7 is selected by physical RAMCFG 6.

For six of the seven used clock ranges, all currently decoded
CONFIG0..CONFIG5 values are identical. The complete 76-byte records are not.

### Lowest range: MCLK 0-540 MHz

```text
8 Gbit:  timing ID 0  @ 0x89746 / CRC32 0x74699E6B
16 Gbit: timing ID 30 @ 0x8A02E / CRC32 0xCD5FFF66

Record +0x2C: 0A -> 4A
Record +0x2D: 04 -> 03
Record +0x2E: 00 -> 90
Record +0x33: A2 -> A3
```

### Intermediate ranges

Ranges 1 through 5 preserve equal decoded fields. Their only complete-record
difference is consistently:

```text
Record +0x33: A2 -> A3
```

`+0x33` is therefore density-correlated in this comparison, but its semantic
name remains unknown. It could encode density, a configuration selector, or
another property that happens to vary with the selected profile.

### Highest used range: MCLK 6301-16383 MHz

The records differ in 25 bytes, including both decoded and unknown regions.
Selected decoded differences are:

```text
8 Gbit:  RC=78 RFC=210 RAS=52 RP=26 CL=24
16 Gbit: RC=90 RFC=240 RAS=60 RP=30 CL=26
```

The wide final interval is a selector range, not a claim that the board runs
at 16,383 MHz.

## Runtime behavior

With the physically installed 8 Gbit devices, RAMCFG 0 initialized normally:

```text
DRAM organization: 256Mx2x16
Framebuffer: 8192 MB
Column count: 2^6
CMD_DDLL A/B/C/D: 145/146/145/147
Command mapping: varied values by pad and subpartition
GFW initialization: complete
```

Selecting the declared 16 Gbit entry with the same physical devices changed
the requested controller geometry and runtime registers:

```text
DRAM organization: 512Mx2x16
Framebuffer: 16384 MB
Column count: 2^7
CMD_DDLL A/B/C/D: 32/32/32/32
Command mapping: uniform value 69
GFW initialization: failed with observed error 0x167
```

Several runtime mode registers also changed at 405 MHz:

```text
MRS2: 0x00200FC0 -> 0x00200FC2
MRS4: 0x0040071F -> 0x0040079F
MRS7: 0x007000C8 -> 0x00700088
MRS8: 0x00800010 -> 0x00800015
MRS9: 0x00900F89 -> 0x00900F8A
```

The experiment intentionally mismatched the physical density. It proves that
the descriptor affects geometry and that equal decoded low-range timings do
not imply equal initialization. It does **not** prove that the 16 Gbit profile
would fail with the intended physical 16 Gbit devices.

## Conclusions supported by the evidence

1. `FULL` means complete timing-map references, not complete functional
   validation.
2. The first 24 decoded bytes cannot establish complete profile equality.
3. Later bytes in the 76-byte record are active research targets.
4. Density selection changes geometry, MRS values, electrical settings, and
   training results in addition to familiar AC timing fields.
5. Runtime calibration output can distinguish successful initialization from
   a configuration that only declares a plausible framebuffer size.

## Conclusions not yet supported

- `+0x33` cannot yet be named as a density field.
- A uniform command-map value of 69 is not yet a universal failure sentinel.
- Timing records alone cannot predict display flicker or P-state stability.
- The exact mapping from timing-record bytes to each MRS value remains
  unresolved.
- The physical 16 Gbit configuration has not been runtime-captured in this
  study.

## Planned physical 16 Gbit conversion

When this board is converted to actual Samsung 16 Gbit devices, repeat the
RAMCFG 6 experiment with the same ROM and tool versions. Preserve enough
evidence to distinguish a valid 16 GB initialization from a framebuffer size
that was merely requested by the descriptor:

1. photograph and record the complete memory-device part number, placement,
   population, and physical RAMCFG resistor state;
2. retain the ROM SHA-256 and confirm RAMCFG 6 selects Samsung entry 7;
3. record cold-boot/POST behavior and whether display output is stable before
   loading a driver;
4. capture GFW/RM initialization, detected geometry, framebuffer size,
   calibration output, command mapping, drive settings, and MRS values at the
   same 405 MHz operating point used by the 8 Gbit baseline;
5. perform bounded memory tests in multiple regions, including addresses above
   8 GB, instead of treating the announced 16 GB capacity as proof;
6. test driver loading, idle/downclock transitions, multiple P-states, monitor
   sleep/wake, and the difference between adaptive and prefer-maximum-
   performance modes;
7. preserve logs for both stable and failing states without assuming that
   `nvmt ts`/`ts2` or `gddrinfo` alone establishes success.

The primary research target is correlating runtime changes with raw timing-
record bytes `+0x2C`, `+0x2D`, `+0x2E`, and `+0x33`. Until repeated controlled
comparisons isolate them, these bytes remain unnamed rather than being labeled
as AC timings, density fields, or direct MRS encodings.

## Alias experiment still available

In this ROM, physical RAMCFG 0 (`L/L/L`) and RAMCFG 8 (`L/L/M`) both translate
to logical entry 1. A future runtime comparison between those two selectors
would isolate whether later initialization uses only the translated logical
group or also consumes the original physical RAMCFG code. Until that test is
performed, the reader labels RAMCFG 8 as a translation alias without claiming
complete runtime equivalence.

## Diagnostic-tool cautions

Two initially plausible indicators were rejected by the working control:

- `nvmt ts` and `ts2` returned the `0x555` pattern after both failed and
  successful initialization, so this value alone is not a GA104 training-fail
  marker in the tested environment;
- `nvmt gddrinfo` returned zeroed vendor information in both states and its
  local help describes GDDR5/GDDR5X mode-register use, so it was not used to
  identify GA104 GDDR6 devices.

The successful control was instead established through complete GFW/RM
initialization, realistic per-channel calibration, MODS execution, and a
bounded framebuffer test outside the initial display region.

## Reproducing the static comparison

```bash
nvidia-bios-reader-cli card.rom --compare-profiles 1 7
```

To save the result:

```bash
nvidia-bios-reader-cli card.rom --compare-profiles 1 7 \
  --output samsung-8gbit-vs-16gbit.txt
```

Contributors testing another ROM should include the ROM hash, VBIOS version,
GPU, Device ID, record offsets, CRC32 values, and all changed byte positions.
