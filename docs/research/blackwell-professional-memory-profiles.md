# Blackwell professional GDDR7 memory profiles

Research snapshot: 2026-09-14. Read-only analysis; no production parser changes, flashing, or hardware experiments.

## Findings in brief

Professional Blackwell ROMs strengthen the empirical GDDR7 organization mapping: code `3` is consistent with four x8 channels per device, and code `2` with two x8 channels / clamshell. Density code `7` is a strong **24 Gbit candidate**, not an officially decoded NVIDIA enumeration. A stored profile is not proof that a board selects or successfully initializes it.

The most important new limit is that legacy `FULL` classification can include records whose first 24 bytes are entirely zero. These records contain nonzero data elsewhere. Consequently, neither the old CONFIG0–CONFIG5 decoder nor the presence of a record establishes valid GDDR7 timings or stability.

See also [organization evidence and consumer controls](gddr7-organization-clamshell.md) and [initial GDDR7 support audit](gddr7-blackwell-support-audit.md).

## Scope and provenance

The local TPU-derived index contains 92,535 catalog rows, including 2,059 GDDR7-labelled rows / 2,015 distinct ROM SHA-256 hashes. The broader organization scan is documented separately. This follow-up inspects 57 professional-device rows plus two ASUS consumer controls: 59 files, 58 distinct hashes, zero extraction errors. Repeated uploads are not independent observations.

All these catalog samples are unverified uploads. Recomputed hashes establish input identity, not NVIDIA authenticity or board compatibility. Model labels are untrusted metadata: actual PCI Device IDs were cross-checked against NVIDIA's [supported-chip list](https://download.nvidia.com/XFree86/Linux-x86_64/580.95.05/README/supportedchips.html).

| Device ID | Normalized family | Catalog rows |
|---|---|---:|
| 2BB1 | PRO 6000 Workstation | 18 |
| 2BB3 | PRO 5000 desktop | 1 |
| 2BB4 | PRO 6000 Max-Q Workstation | 6 |
| 2BB5 | PRO 6000 Server | 2 |
| 2C31 | PRO 4500 desktop | 4 |
| 2C33 | PRO 4000 SFF | 2 |
| 2C34 | PRO 4000 desktop | 4 |
| 2C38 | PRO 5000 Laptop | 2 |
| 2C39 | PRO 4000 Laptop | 4 |
| 2D30 | PRO 2000 desktop | 4 |
| 2D39 | PRO 2000 Laptop | 3 |
| 2DB8 | PRO 1000 Laptop | 3 |
| 2DB9 | PRO 500 Laptop | 4 |

For example, some catalog rows called PRO 6000 are actually `2C38` or `2D39`. Do not infer topology by combining such names with advertised capacities.

## Organization and density are separate fields

For the first little-endian descriptor DWORD `d`, the empirical extraction is:

```text
type         = d & 0xF
vendor       = (d >> 12) & 0xF
density_code = (d >> 20) & 0xF
organization = (d >> 24) & 0x7
upper_flags  = d >> 27
```

Observed GDDR7 type is `0xD`; sampled vendor codes include Samsung `1`, SK hynix `6`, and Micron `15`. Do not confuse this VBIOS type with driver/API enums.

GDDR7's two-channel mode and clamshell topology are explicitly represented in the [JESD239 table of contents](https://store.accuristech.com/ecia/products/preview/2581661). [SmartDV's GDDR7 implementation description](https://www.smartdvtech.com/products/gddr7-vip/) describes four independent channels and four-/two-channel initialization. These sources describe memory architecture, **not** NVIDIA's descriptor bit assignments.

Recommended research labels:

| Code | GDDR7 interpretation | Evidence level |
|---|---|---|
| Organization 3 | Four x8 channels; aggregate x32 device interface | Strong empirical inference |
| Organization 2 | Two x8 channels; aggregate x16 device interface; clamshell configuration | Strong empirical inference |
| Density 6 | 16 Gbit | Consistent with consumer controls |
| Density 7 | 24 Gbit candidate | Strong cross-model inference; active-profile validation missing |

This does not equate x32 with a single-sided PCB, or x16 with a guaranteed physical layout of an unknown board. Manufacturer alone cannot establish organization: Samsung occurs in both modes. Upper descriptor flags also must not be folded into organization: `0x23` and `0x03` in the high byte both yield organization `3`, but different upper flags. Their meanings remain unresolved.

## Evidence for density code 7

The 26 PRO 6000 Workstation, Max-Q, and Server rows all have Samsung entry 5 with descriptor `0270144D`: density `7`, organization `2`, and complete legacy timing-reference coverage. A physical PRO 6000 teardown counted 32 memory packages totaling 96 GB, implying 3 GB / 24 Gbit per package. [GamersNexus teardown](https://gamersnexus.net/gpus/nvidia-rtx-pro-6000-blackwell-benchmarks-tear-down-thermals-gaming-llm-acoustic-tests)

The laptop cross-check is useful but not conclusive. NVIDIA specifies PRO 5000 Laptop with 24 GB on a 256-bit interface and PRO 4000 Laptop with 16 GB on the same width. [Official comparison](https://www.nvidia.com/en-gb/products/workstations/professional-laptops/compare/)

Both families contain Samsung entry 5 `2370144D`, density `7`, organization `3`, with complete reference coverage. Thus, the presence of that entry alone cannot distinguish the 24 GB board from the 16 GB board. In the ten sampled lower-end professional laptop rows (`2D39`, `2DB8`, `2DB9`), the same density-code-7 entry is PARTIAL, with its highest range referencing `FF`.

Samsung independently documents actual [24 Gbit GDDR7 devices](https://news.samsung.com/global/samsung-develops-industrys-first-24gb-gddr7-dram-for-next-generation-ai-computing). This establishes that the proposed density is physically real, but does not map the VBIOS enum.

**Do not extrapolate a power-of-two density formula to code 7.** A future parser should retain the raw code and mark the proposed 24 Gbit interpretation as inferred until a known board's active descriptor is correlated with package markings and initialized capacity.

## Timing-table geometry and reproducible offsets

All 59 samples have 105-byte timing records under table version `0x20`; 44 have 96 records and 15 have 110. All maps have version `0x11` and ten range entries. Most use header prefix `11 1D 70 5C 0D 0A`: 112-byte base plus 13 extensions of 92 bytes, giving 1,308 bytes per map entry. PRO 5000 sample 283527 instead has 16 extensions, giving 1,584 bytes. Do not hardcode either count.

The memory-information tables use version `0x10`, seven-byte headers, 22-byte records, and 16 slots. Some slots carry type `0xF`; the analysis excludes those from memory-profile comparisons while preserving them in the underlying corpus evidence.

All offsets below are absolute within the corresponding hashed input file, not universal offsets:

| TPU catalog ID | Role | Timing map | Timing table |
|---|---|---|---|
| 283591 | PRO 6000 Max-Q | 0x6DB5C | 0x70E91 |
| 278794 | PRO 6000 Server | 0x73A5C | 0x76D91 |
| 283527 | PRO 5000 desktop 48 GB | 0x7465D | 0x7845A |
| 282929 | PRO 5000 Laptop | 0x6F15C | 0x72491 |
| 283595 | PRO 4000 Laptop | 0x6E25C | 0x71591 |

Sample 283591, version `98.02.47.00.02`, SHA-256:

```text
ccc1870e8caca6231c7922fc7269f291046c5ff4c2bad22dcec4602404d66137
```

Its entry 5 descriptor starts at decimal offset 236914. Its ten raw ranges and selected record IDs are:

| Raw range | Record ID |
|---|---:|
| 0–540 | 49 |
| 541–1249 | 51 |
| 2005–4999 | 52 |
| 5000–7499 | 54 |
| 7500–8500 | 56 |
| 8501–9500 | 58 |
| 9501–11349 | 60 |
| 11350–13249 | 61 |
| 13250–14749 | 62 |
| 14750–19999 | 63 |

These are raw map bounds, **not confirmed MHz, data rates, or P-state names**. Several laptop samples use a 14799/14800 boundary instead of 14749/14750. Range indexes are not P-state numbers.

## Why FULL cannot mean decoded or stable

In 45 of 59 samples, at least one referenced, nonzero record has an all-zero first-24-byte prefix. In sample 283591, entry 5 references record 52 at `0x723EB` for raw range 2005–4999. Within its 105 bytes, only these bytes are nonzero:

```text
record + 0x31 = 0x21
record + 0x65 = 0x92
record + 0x66 = 0x25
```

This is not an all-zero record, and its ID is not `FF`. However, applying the old six-DWORD decoder would produce zeros throughout the prefix. Possible explanations include a special record form, indirection, defaults, or a placeholder; none is established. A parsing/layout error must also remain a hypothesis until independently validated.

The research script reproduces the legacy coverage convention: FULL means every non-unused raw range references an in-bounds, nonzero record; PARTIAL mixes valid records and `FF`; EMPTY means all used references are `FF`; INVALID covers out-of-bounds or zero records. It does **not** validate initialization, timing semantics, training, or runtime use.

Keep separate dimensions in a future UI: `Reference coverage`, `Record interpretation`, and `Active profile: unknown`. Do not turn these observations into a claim that factory cards have defective timings.

## Byte-level comparisons

All comparisons use complete 105-byte records, not just the displayed CONFIG prefix. IDs themselves are local indexes, not globally meaningful timing names.

1. **ASUS 5060 Ti 8 GB (277002) vs 16 GB (275774):** all 96 corresponding timing records are identical. Selected record IDs also match for shared profiles. The earlier descriptor comparison found organization changes in three entries. Capacity/topology changes can therefore occur without changing this timing-record table; other initialization data can still differ.
2. **PRO 5000 Laptop (282929) vs PRO 4000 Laptop (283595):** 87/96 corresponding records match. For Samsung entries 1, 5 and 8, all ten selected records match byte-for-byte, with matching range bounds. The 24 GB and 16 GB model labels do not imply different Samsung timing payloads here.
3. **PRO 6000 Max-Q (283591) vs Server (278794):** 91/96 records match. Samsung entry 5 differs at raw ranges 0–540, 541–1249, 5000–7499, 11350–13249 and 13250–14749. Its other five ranges match. Entries 1 and 8 match across all ranges.
4. **PRO 6000 Max-Q vs PRO 5000 desktop (283527):** 74 of the first 96 corresponding records match; the latter table has 110 records. Entry 5 also changes density code, and additional descriptor bytes differ. This is not a controlled density-only experiment.

Example from comparison 3: record 49 differs at relative offsets `02, 2C, 2D, 3C, 65, 66`; record 51 differs at `00, 02, 04, 0C, 2B, 39, 58, 5A, 65, 66`. Some changes lie beyond CONFIG0–CONFIG5. There is no justified ranking of these profiles as tighter, looser, faster, or more stable: byte values may contain packed fields, flags, or encoded units.

## Remaining gaps and next useful samples

- A board with known GDDR7 package markings, physical topology, active RAMCFG, and initialized memory capacity would validate descriptor interpretation much more directly than more uploads of the same ROM.
- PRO 5000 desktop 48/72 GB is a promising density control. NVIDIA now documents both capacities in its [current datasheet](https://www.nvidia.com/content/dam/en-zz/Solutions/products/workstations/professional-desktop-gpus/rtx-pro-5000-blackwell/workstation-datasheet-blackwell-rtx-pro-5000-gtc25-spring-nvidia-3658700.pdf), but this selected local corpus contains only one 48 GB sample. The targeted web search did not establish a downloadable 72 GB ROM. No conclusion about that missing ROM is made.
- Decode the complete 105-byte layout and its possible variants before exposing named GDDR7 timings. Table version alone is insufficient validation.
- Establish clock units and runtime range selection before converting to MHz or assigning P0/P2/P8.
- Inspect training, partition and script references independently. Shared timing records do not prove those paths are equal.
- Translation-table references identify declared selections, not the active electrical strap or a guarantee of successful initialization.

## Reproduction

Run the existing organization scanner against the local archive, then:

```sh
python -B tools/research/analyze_blackwell_profiles.py reports/gddr7-organization/corpus.json reports/blackwell-pro/profiles-with-comparisons.json
```

Use a new output filename for each run: the script deliberately refuses to overwrite evidence. It rehashes each input against the corpus scan, performs bounded reads, records errors, and exports absolute offsets, raw bytes, hashes, profile coverage, and pairwise changed-byte locations. ROMs are never modified or redistributed. Full local JSON reports are ignored by Git; the scripts and this note are the shareable research artifacts.

The evidence is useful for an **experimental structural GDDR7 reader**, not yet a validated GDDR7 timing decoder. No production behavior was changed during this audit.
