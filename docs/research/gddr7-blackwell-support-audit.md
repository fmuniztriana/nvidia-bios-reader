# GDDR7 / Blackwell support audit

Date: 2026-09-14. Scope: read-only research, not a support announcement.

Follow-up: the [GDDR7 organization/clamshell corpus study](gddr7-organization-clamshell.md) subsequently compared 2,059 local catalog records. It establishes a high-confidence empirical interpretation of organization `0x2` versus `0x3`. Statements below about organization being unresolved describe the initial single-sample audit; the follow-up supersedes that uncertainty, but does not validate timing semantics or runtime selection.

## Conclusion

**NBR can recover useful structures from at least one Blackwell GDDR7 firmware sample, but it does not currently provide validated GDDR7 interpretation.** It should not advertise GDDR7 support or use its current decoded timing values to rank GDDR7 profiles.

This is not simply an unsupported file that fails to open. The tested executable successfully locates memory descriptors, a translation table, clock-range mappings and timing records, then applies some older-generation interpretations without a suitable compatibility check. Consequently, a successful parse and a `FULL` label can be misleading.

The most useful immediate work would be an experimental **raw/structural reader with explicit uncertainty**, followed by separately validated GDDR7 decoding. Renaming `Unknown 0xD` alone would not solve the problem.

## Method and scope

Audited source: `src/nvbios_reader.cpp`, repository base commit `3917cfa94671250f45cffb03c9de02b0c7ec228f`. Executable tested: `build/icon-preview/Release/nvidia-bios-reader-cli.exe`, reporting `0.4.0-internal.2`. The GUI and CLI share the parsing core; this test exercised the CLI, not a live GUI or GPU. It does not establish the exact behavior of every older public release.

Work performed:

1. Read the current identification, descriptor, pointer, timing and report-generation code.
2. Consult NVIDIA architecture documentation, public BIOS documentation, open driver definitions and memory-vendor material.
3. Download and hash-check an official Dell firmware update package.
4. Inspect its embedded bytes without executing the updater.
5. Run NBR on a bounded analysis window and independently inspect the table headers, descriptors, references and raw records with Python.

No firmware was flashed, no GPU registers were written, and no runtime stability test was performed. No production parser changes were made for this audit. Existing GUI/icon work was left untouched.

## Reference sample and provenance

The [Dell CD0HN firmware page](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=cd0hn) identifies an RTX 5070 update for Alienware 16X Aurora AC16251, version `98.06.2B.00.1F`, A01, released November 17, 2025. Its release note specifically mentions Micron VRAM timing tuning. That makes it a useful research reference, but does not disclose which bytes changed or explain any particular record.

- Package: `Alienware_16X_Aurora_AC16251_RTX5070_98062B001F.exe`
- [Official download](https://dl.dell.com/FOLDER13737823M/1/Alienware_16X_Aurora_AC16251_RTX5070_98062B001F.exe)
- Package SHA-256: `275affdb658b4e8ba13b041dbc118931f614954f1e057bcfc8c8318362ed05ef`
- Analysis window starts at package offset `0x1683C34`, length `0x200000`.
- Window SHA-256: `0e278ed7bf5ff06276dd71a351eea69e58032dacba95c5c4a62ade821d88dd64`
- NVIDIA legacy PCI image inside the window: offset `0x1200`, length `64000`, device `10DE:2D58`, code type `0`.
- NBR reads BIOS chip code `0x9806`, but reports the chip as `Unknown`.

**The full standalone VBIOS boundary has not been established. The two-megabyte window is a research input, not a flashable ROM.** It may include surrounding package content. All reported table spans are within the window, and their local structures were checked independently. Do not rename it to a ROM and flash it.

This is one laptop firmware sample. It does not establish universal compatibility for desktop GeForce, workstation Blackwell, other manufacturers, or later firmware revisions. Its legacy PCI image also demonstrates why one should not assume that every Blackwell firmware lacks an image NBR can recognize merely because the platform uses UEFI.

## What the existing reader actually handles

| Layer | Observed result | Interpretation |
|---|---|---|
| PCI identity | `10DE:2D58` recovered | Useful structural result |
| GPU name | `Unknown`, chip code `0x9806` | Identification mapping missing |
| Memory type | `Unknown 0xD` | Strong sample-specific candidate for GDDR7 encoding |
| Manufacturer | Samsung, Hynix and Micron codes appear | Consistent with existing vendor namespace; not physical-population proof |
| Density | Code `0x6` prints 16 Gbit; `0x7` unknown | New/unvalidated codes must be preserved |
| Organization | Code `0x3` unknown | Cannot infer package width or clamshell from the current decoder |
| Translation | Thirteen bytes recovered | Byte mapping observed; electrical H/M/L interpretation not runtime-validated here |
| Clock-range mapping | Ten records found | Raw boundaries and IDs are useful; displayed clock units are not validated |
| Timing records | 96 records, 105 bytes each | Structural extraction works |
| Timing names / cycles | Older masks applied | Not validated for GDDR7 |
| `FULL` / `PARTIAL` | Existing heuristic produces labels | Reference coverage, not semantic completeness or stability |

## Table locations and manual reproduction

All offsets below are relative to the **analysis window**, not the beginning of the Dell executable. Add `0x1683C34` to obtain package offsets.

| Structure | Offset | Relevant bytes / geometry |
|---|---:|---|
| BIT signature | `0x1FF0` | `FF B8 42 49 54 00` |
| BIT M payload | `0x210D` | Translation count and memory pointers |
| Translation array | `0x5A25` | `00 01 02 03 04 05 06 07 00 01 02 03 04` |
| Memory information | `0x5B13` | `10 07 16 10`: version `0x10`, header 7, record length 22, count 16 |
| Clock/timing map | `0x3985C` | `11 1D 70 5C 0D 0A`: version `0x11`, header 29, base 112, extension 92, 13 extensions, 10 records |
| Timing records table | `0x3CB91` | `20 06 69 0C 00 60`: version `0x20`, header 6, base 105, extension count 0, record count 96 |

The M pointer at `0x210E` contains `0x4825`; adding legacy base `0x1200` gives `0x5A25`. The following pointer contains `0x4913`, resolving to `0x5B13`. NBR uses an intervening UEFI-image adjustment of `0x18000` for the relevant longer logical pointers.

To inspect the recovered data manually:

```text
Memory descriptor entry i (zero-based): 0x5B13 + 7 + i * 22
Map range r:                           0x3985C + 29 + r * 1308
Candidate timing ID for group g:        range_offset + 112 + g * 92
Timing record t:                       0x3CB91 + 6 + t * 105
```

The map stride is `112 + 92 * 13 = 1308` (`0x51C`). All formulas are for this verified sample, not universal offsets. The header versions look familiar while the lengths/layout details differ from previous generations. **Version checks alone are not a sufficient compatibility boundary.**

The ten raw range boundaries are:

```text
0–540       541–1249       2005–4999      5000–7499
7500–8500   8501–9500      9501–11349     11350–13249
13250–14799 14800–19999
```

Do not label these P8/P5/P2/P0, GPU-Z clocks, or effective transfer rates without validating their domain and runtime selection. A timing-map interval is not itself a P-state.

## Memory profiles and reference coverage

Names/density interpretations in this table follow the old descriptor bit fields and are qualified accordingly. `0xD` is observed throughout the non-skip descriptors. There are 16 declared slots, eight non-skip descriptors, and seven distinct non-skip descriptors referenced by the declared translation array. The report also lists skip slots.

| Entry (1-based) | Descriptor | Vendor code interpretation | Density code | Timing IDs for the ten ranges | NBR label |
|---:|---|---|---|---|---|
| 1 | `2360100D` | Samsung | `6` → 16 Gbit under existing mapping | 0,2,4,5,7,9,11,13,14,14 | FULL |
| 2 | `0360611D` | Hynix | `6` | 16,18,20,21,24,25,27,29,30,31 | FULL |
| 3 | `0360F22D` | Micron | `6` | 32,34,36,37,39,41,43,45,46,47 | FULL |
| 4 | `0360133D` | Samsung | `6` | FF in all ten ranges | EMPTY |
| 5 | `2370144D` | Samsung | `7`, unknown | 48,50,52,55,37,57,59,61,62,FF | PARTIAL |
| 6 | `0360655D` | Hynix | `6` | 16,18,20,21,23,25,27,29,30,30 | FULL |
| 8 | `0360177D` | Samsung | `6` | 1,3,4,6,8,10,12,13,14,15 | FULL |
| 9 | `0360688D` | Hynix | `6` | 17,19,20,22,24,26,28,29,30,31 | FULL, unmapped by declared translation |

Entry 7 and entries 10–16 have the skip type under the existing interpretation. An unmapped descriptor is not proof that the hardware can never select it through another mechanism; a declared mapping is not proof that a board physically supports that configuration.

### Concrete limitation of `FULL`

Micron entry 3 references timing ID 47 for raw range `14800–19999`. Record 47 starts at `0x3DEDE`. Its first 24 bytes are zero, but four later bytes are nonzero:

```text
record-relative +0x31 = 21
record-relative +0x33 = E0
record-relative +0x65 = 92
record-relative +0x66 = 25
all other bytes = 00
```

Record 52 at `0x3E0EB`, referenced by Samsung entry 5 at raw range `2005–4999`, has the same complete 105-byte contents.

NBR's whole-record nonzero check accepts these as present. Its legacy decoder then prints zero for every field it reads from the first 24 bytes. This is reproducible evidence that **nonzero record bytes do not establish usable decoded timings**.

We have not determined whether these records are placeholders, special commands, indirect/default behavior, or something else. Do not reclassify them as defective firmware or a demonstrated crash cause. The Micron range is not even proven to be reachable on the shipping configuration. Keep coverage and semantic validity separate.

## Code-level causes

The following references describe the audited source, not a proposed fix:

- `chip_from_bios_code`, around line 298: TU/GA/AD mappings, no Blackwell mapping.
- `memory_type`, around line 349: recognizes GDDR6 `0x9` and GDDR6X `0xA`, not observed `0xD`.
- `memory_density`, around line 379: stops at code `0x6` / 16 Gbit.
- `memory_organization`, around line 392: no validated GDDR7 organization mapping.
- `device_clock_divisor`, around line 415: unknown memory types fall back to divisor 1. Therefore the sample gets an apparent device-clock range as high as 19999 MHz, without supporting evidence for that conversion.
- `parse_memory`: respects record length but does not validate a Blackwell-specific descriptor schema.
- `parse_timings`, around line 573: requires map version `0x11`, but does not validate the complete memory-generation/timing-layout combination; timing-table version is read without selecting a dedicated semantic decoder.
- `decode_timing_fields`, around line 664: extracts the same six dwords with fixed masks.
- `timing_status`, around line 721: evaluates FF, index bounds and whole-record zero content, not validated timing semantics.
- Call sites around lines 992 and 1190: record length of at least 24 bytes enables named-field decoding, without a GDDR7 compatibility check.
- `analyze`, around line 755: currently requires an NVIDIA legacy PCI image. Other packaging arrangements still need testing.

Some legacy masks may turn out to remain valid. This audit does **not** prove every displayed number is wrong. It proves that the program currently lacks enough validation to claim those names, units and meanings are correct for GDDR7.

## Why GDDR7 needs its own validation

NVIDIA's [RTX Blackwell architecture whitepaper](https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf), memory-subsystem section, describes PAM3 signaling, more independent memory channels, and changes to clocking, training and reliability mechanisms. These are reasons to check rather than assume inherited mappings; they do not by themselves reveal VBIOS bit masks.

The [Micron GDDR7 catalog](https://in.micron.com/products/memory/graphics-memory/gddr7/part-catalog) includes 16 Gb and 24 Gb x32 parts. A decoder therefore needs non-power-of-two capacities and must distinguish package width from channel organization. **Do not assign observed density code `0x7` to 24 or 32 Gbit solely by extrapolating the previous enum.** Likewise, organization code `0x3` is not yet proof of x32 or a particular PCB topology.

NVIDIA's [open driver RAM-type definitions](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/61dcc93722ecb418bb5f2e00923f05b4b8051dd1/src/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080fb.h#L400) include GDDR7 as `0x15`. This is a **driver API enum**, not the four-bit VBIOS memory-type field. The namespaces already differ for GDDR6. Copying `0x15` into the VBIOS decoder would be incorrect; the actual sample's candidate type code is `0xD`.

The public [Memory Clock Table specification](https://nvidia.github.io/open-gpu-doc/MemoryClockTable/MemoryClockTable.html) is useful structural background, not a complete GDDR7 schema. It describes a 14-bit frequency field in its documented layout. This sample contains a raw upper boundary of 19999, above 16383. That discrepancy needs investigation; neither blindly masking the high bits nor automatically calling the raw value MHz is justified here.

The public sources inspected did not provide a complete GDDR7 VBIOS timing-field/register decoder. This is a bounded search result, not a claim that no such information exists anywhere.

## What can be compared today?

Safe research comparisons include raw descriptors, table geometry, reference IDs, FF locations, identical/different byte records and byte-level differences within a matched layout. For example, Samsung entries 1 and 8 already select different records in several low ranges.

Those differences are not sufficient to call a profile tighter, looser, faster or more stable. That requires validated field meanings, units, clock domains, mode-dependent encodings and runtime behavior. Full-record byte equality is stronger than equality of six decoded words, but still does not prove identical training, voltage, initialization or transition behavior elsewhere in the firmware.

For the project's memory-mod investigation, this distinction is essential: a missing reference can be evidence of incomplete mapping, while a present record is not a guarantee that increased density or low-power transitions will work.

## Recommended implementation sequence

1. **Guard unsupported semantics first.** Keep structural output but mark GDDR7 timing interpretation `UNVERIFIED`. Avoid speculative named timings and device-clock conversion. This applies to both GUI and CLI. Unknown formats should not silently acquire old-generation confidence labels.
2. **Add experimentally verified identity mappings.** Preserve raw codes beside names. Validate chip IDs, type `0xD`, density and organization on several independent samples before generalizing.
3. **Separate three dimensions:** reference coverage, recognized timing schema, and runtime validation. Add a warning for zero legacy-prefix/nonzero-tail records without declaring them invalid by fiat.
4. **Build a test matrix:** desktop and laptop Blackwell, Samsung/Hynix/Micron, 16 Gb and confirmed 24 Gb populations, different firmware revisions and package types. Include truncation/bounds tests and preserve all older-generation regression cases.
5. **Find a before/after pair for the same Dell board.** Its Micron-specific update note makes this a promising differential-analysis target. We only acquired the newer sample, so no update delta is claimed here.
6. **Validate semantics against real hardware.** Match saved ROM, chip markings, selected configuration, observed memory clocks and documented/read-only runtime data. Investigate training and transitions separately. Do not flash this research extraction or test random straps to establish basic parsing support.

## Reproduction and retained evidence

The standard-library-only script [audit_dell_gddr7.py](../../tools/research/audit_dell_gddr7.py) hash-locks the official package, extracts the analysis window and exports raw evidence as JSON. It is intentionally sample-specific, with explicit offsets; it is not an independent general-purpose VBIOS parser and does not independently establish every pointer's semantic name.

From the repository root, after downloading the official package without running it:

```powershell
python tools/research/audit_dell_gddr7.py "PATH/Alienware_16X_Aurora_AC16251_RTX5070_98062B001F.exe" reports/gddr7-audit
build/icon-preview/Release/nvidia-bios-reader-cli.exe reports/gddr7-audit/dell-5070-analysis-window.bin --timings -o reports/gddr7-audit/nbr-report.txt
```

Use an unused output directory: the script refuses to overwrite evidence files. The executable path above is the local audited build; another build may have different behavior.

Local evidence from this run is retained under `reports/gddr7-2026-09-14/` (ignored by Git): `audit.json`, `nbr-report.txt` and the analysis window. The original package remains in the parent workspace's `tmp/gddr7-research/`. Firmware binaries are not added to the repository.

The [ASUS RTX 5070 Ti TechPowerUp page](https://www.techpowerup.com/vgabios/273564/asus-rtx5070ti-16384-250120) was also inspected and lists GDDR7 Hynix/Samsung support. Its binary download was blocked, so it is **metadata corroboration only**, not a second binary test. This audit should be expanded with additional legitimately acquired full ROM dumps before any general GDDR7 support claim.
