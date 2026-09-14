# GDDR7 organization and clamshell profiles

Follow-up: [professional Blackwell profiles, density-code evidence and complete raw timing comparisons](blackwell-professional-memory-profiles.md) expands this study with 57 professional-device samples. It documents why complete reference coverage does not establish a decoded or stable GDDR7 profile.

## Conclusion

The local VBIOS corpus provides strong empirical evidence for the following interpretation of the three-bit organization field in the observed GDDR7 memory descriptor layout:

| Memory type | Organization code | Supported interpretation | Evidence level |
|---|---:|---|---|
| `0xD`, GDDR7 | `0x3` | Four-channel configuration; x32 aggregate device data interface | High-confidence inference |
| `0xD`, GDDR7 | `0x2` | Two-channel configuration; x16 aggregate device data interface, used for clamshell | High-confidence inference |

This advances beyond the single-sample uncertainty recorded in the [initial GDDR7 audit](gddr7-blackwell-support-audit.md). It is a profile-format finding, not a determination of the active configuration on a physical card, a runtime validation, or a complete GDDR7 timing decoder.

The strongest comparison is RTX 5060 Ti 8 GB versus 16 GB. All 57 catalog records for the 8 GB model contain only organization `0x3` among their non-skip GDDR7 descriptors. All 188 records for the 16 GB model include organization `0x2` as well as retained `0x3` profiles. Deduplicating complete ROMs leaves 56 and 184 unique SHA-256 values, respectively; there is no identical ROM hash shared between those two groups.

The physical interpretation is supported independently by the MSI RTX 5060 Ti 16 GB teardown: eight memory packages, four on each side, in clamshell. NVIDIA lists 8 GB and 16 GB RTX 5060 Ti configurations on the same 128-bit interface. These sources establish the product topology context, not the numerical BIOS enum itself. [1][2]

## Corpus and provenance

The read-only comparison was performed on September 14, 2026. The local collection index contains 92,535 records; 2,059 are labelled GDDR7. All 2,059 corresponding files were available and their memory tables were extracted successfully. They represent 2,015 unique complete-file SHA-256 values.

| Integrity / layout check | Result |
|---|---:|
| GDDR7-labelled catalog rows inspected | 2,059 |
| Unique ROM SHA-256 values | 2,015 |
| SHA-256 matches against stored catalog hashes | 2,047 |
| Missing stored SHA-256, actual hash computed | 12 |
| Stored SHA-256 mismatches | 0 |
| Parsed memory table version | `0x10` in all 2,059 |
| Memory descriptor record length | 22 bytes in all 2,059 |
| Non-skip memory-type codes | Only `0xD` |
| Non-skip organization codes | Only `0x2` and `0x3` |

There are 14,079 non-skip descriptors across catalog rows: 13,293 with organization `0x3`, and 786 with `0x2`. These are descriptive corpus counts, not independent hardware observations. Duplicate uploads, related firmware builds and common NVIDIA templates are correlated evidence.

All sampled catalog records are tagged as **unverified uploads**. Matching a stored hash verifies identity with the archived input, not manufacturer authenticity or correctness of the uploader's capacity label. Some catalog names are plainly inconsistent or report zero capacity; they are not used as the primary topology controls.

The source index is located at:

```text
<local-archive>/Video-BIOS-Collection/index.json
```

Its SHA-256 at analysis time is:

```text
e3a886d6301d48b849e9eaeb676eb6e93f3293cf9e61b9534b665e03118b5360
```

An earlier copied index contains 2,047 GDDR7 entries. The results in this report use the larger source collection, not that copy. The indexed ROM paths resolve relative to the source index directory.

## Controlled ASUS descriptor comparison

The following files share GPU device ID `10DE:2D04` and the same memory-table geometry, but have different board/subsystem identities. They are not a controlled firmware edit of a single board; the descriptor comparison is nevertheless particularly clean. Archived TechPowerUp metadata identifies the capacities and firmware versions. The four selected ASUS/MSI files also match both the MD5 and SHA-1 recorded from their TPU pages. [3][4][5][6]

| Attribute | ASUS 8 GB | ASUS 16 GB |
|---|---|---|
| TPU ID | `277002` | `275774` |
| VBIOS version | `98.06.1F.80.39` | `98.06.1F.40.43` |
| Subsystem | `1043:8A30` | `1043:8A06` |
| Memory table offset | `0x39D13` | `0x39D13` |
| Header / record / count | 7 / 22 / 16 | 7 / 22 / 16 |
| First descriptor offset | `0x39D1A` | `0x39D1A` |

Across the complete array of sixteen 22-byte descriptor records, only three bytes differ:

| Entry | Vendor interpretation | Descriptor offset | 8 GB dword | 16 GB dword | Differing byte offset |
|---:|---|---:|---|---|---:|
| 1 | Samsung | `0x39D1A` | `0360100D` | `0260100D` | `0x39D1D`: `03` → `02` |
| 2 | Hynix | `0x39D30` | `0B60611D` | `0A60611D` | `0x39D33`: `0B` → `0A` |
| 6 | Hynix | `0x39D88` | `0360655D` | `0260655D` | `0x39D8B`: `03` → `02` |

In each changed dword, XOR is `0x01000000`: bit 24 changes. The existing three-bit organization extraction changes from 3 to 2. Vendor, density, selector and remaining bits are unchanged within those descriptors. Every other byte in the descriptor array is identical. **This does not mean the complete VBIOS files differ by only three bytes**; tables elsewhere were not reduced to this delta.

For entry 1, the bytes visible in a hex editor are:

```text
8 GB:  0D 10 60 03
16 GB: 0D 10 60 02
```

The fields used for the research comparison are:

```text
type         =  descriptor        & 0xF
vendor       = (descriptor >> 12) & 0xF
density      = (descriptor >> 20) & 0xF
organization = (descriptor >> 24) & 0x7
```

Both entry-1 profiles retain vendor code `1` and density code `6` (the existing 16 Gbit interpretation). Thus the organization distinction is not an inference from the manufacturer name or from changing density.

The translation arrays are identical:

```text
00 01 02 03 04 05 06 07 00 01 02 03 04
```

Under the existing zero-based group mapping, physical codes 0 and 8 select entry 1; 1 and 9 select entry 2; and 5 selects entry 6. No Blackwell electrical H/M/L codebook was measured during this research, so these byte-level selectors should not be turned into soldering instructions.

## Repetition across manufacturers

The same first-entry and Hynix-entry transition occurs in MSI files `276947` (8 GB, `98.06.1F.80.14`) and `275849` (16 GB, `98.06.1F.00.C3`). The archived board messages identify PG152 SKU 15 and SKU 10 respectively. [5][6]

Across all 188 catalogued RTX 5060 Ti 16 GB ROMs:

- 86 contain organization `0x2` in entries 1, 2 and 6.
- 102 contain organization `0x2` in entries 1, 2, 3 and 6.
- Entry 1 is organization `0x2` in all 188.

Across all 57 RTX 5060 Ti 8 GB ROMs, no non-skip descriptor contains organization `0x2`; entry 1 is `0x3` in all 57. The pattern repeats across ASUS, MSI, Gigabyte, PNY, Palit and other catalog manufacturer labels.

The broader corpus also contains organization `0x2` in workstation-labelled firmware, including all 24 rows explicitly labelled RTX PRO 6000 Blackwell 96 GB. Both organization codes coexist there as well. Vendor codes associated with `0x2` across the corpus include Samsung, Hynix and Micron. This supports a vendor-independent organization interpretation, but workstation catalogue labels are corroboration rather than a substitute for a matched teardown/runtime test.

## Why an organization label is not a board-level verdict

### Professional Blackwell cross-check

A subsequent read-only check of the same corpus found these professional-labelled examples. Capacity/model labels below are from archived TPU metadata; they are not newly measured hardware capacities.

| TPU ID | Catalog model | Device ID | Examples of non-skip organization `0x2` |
|---|---|---|---|
| `283196` | RTX PRO 2000 Blackwell 16 GB | `2D30` | Samsung entry 1, Hynix entry 6 |
| `282919` | RTX PRO 4000 Blackwell 24 GB | `2C34` | Samsung entry 1, Hynix entry 6 |
| `283527` | RTX PRO 5000 Blackwell 48 GB | `2BB3` | Samsung entries 1, 5, 8; Hynix entry 6 |
| `283450` | RTX PRO 6000 Blackwell 96 GB | `2BB4` | Samsung entries 1, 5, 8 |

All four files also retain organization `0x3` profiles. The PRO 6000 example contains Samsung descriptor `0270144D` in entry 5: memory type `0xD`, density code `0x7`, organization `0x2`. Density code `0x7` remains separately under investigation; this does not identify the active profile.

For an independent physical reference, GamersNexus counted 16 memory packages on the back and 32 memory locations total on its RTX PRO 6000 Blackwell Workstation Edition, corresponding to 3 GB modules and 96 GB total. NVIDIA independently specifies 96 GB GDDR7 for that product. This is substantially better evidence of a clamshell reference board than capacity alone. It corroborates the organization inference without linking the reviewed board to a particular archived ROM or proving a runtime selector. [9][10]

Professional desktop and laptop entries must not be pooled by marketing-name substring. Some unverified uploads are evidently mislabeled: for example, an entry named RTX PRO 6000 Blackwell 16 GB has device `2D30`, also found under RTX PRO 2000 Blackwell 16 GB; other PRO 6000 labels have `2C38` or `2D39`. Use exact device IDs, board metadata and independent product identification before interpreting their topology. These questionable labels were not used as primary clamshell controls.

### Profile versus active configuration

A 16 GB ROM can still contain four-channel profiles. In the ASUS pair, entries 3, 4, 8 and 9 remain organization `0x3` in both files. Some are referenced by the translation array. Their presence does not imply that the 16 GB board normally uses them or that selecting them is valid with its physical assembly.

Likewise, entry 1 being mapped to physical selectors does not establish which selector is active. A saved ROM contains configuration options; it is not a live report of populated packages, electrical straps or successful initialization.

Consequently, these are defensible outputs:

```text
Profile organization: 2-channel / clamshell (inferred from corpus)
Raw organization code: 0x2
Active profile: Unknown from ROM alone
```

This is not defensible from a matching descriptor alone:

```text
This physical card is currently running eight memory chips in clamshell.
```

An actual module marking identifies a part and its supported modes, not necessarily its selected operating mode. Combining known package density, package count and bus width establishes a much better physical control. With 16 Gbit packages, four packages using an aggregate x32 interface give 8 GB on 128 bits; eight packages using an aggregate x16 interface give 16 GB on the same bus. This arithmetic is conditional on the stated package density and topology, not a universal inference from capacity alone.

## GDDR7 terminology

JEDEC's publicly indexed GDDR7 document preview explicitly lists two-channel mode, four-channel versus two-channel system views, and clamshell topology. SmartDV's GDDR7 verification-IP description confirms four independent x8 channels and support for both operating modes. [7][8]

For GDDR7, x32 and x16 in the proposed labels refer to the aggregate data-interface width of a device in the selected mode, **not** the width of one independent x8 channel. This should be explicit in a tooltip. These labels must not be ported blindly to GDDR6/GDDR6X, where NBR's existing channel/organization wording uses different conventions.

It is preferable to display `4-channel` and `2-channel / clamshell` rather than equate the mode with `single-sided` or `double-sided`. The former describes the memory configuration; the latter describes physical PCB placement, which the saved ROM does not independently prove.

## Implementation boundary

The evidence now supports introducing a memory-type-specific GDDR7 organization mapping into an experimental NBR build:

```text
type 0xD, organization 0x3 -> 4-channel / x32 device interface (inferred)
type 0xD, organization 0x2 -> 2-channel / x16 device interface, clamshell (inferred)
other organization codes -> Unknown, preserve raw value
```

The qualifier should indicate reverse-engineered/corpus-derived evidence, not that an official NVIDIA enum has been found. The result is strong enough to be useful; it does not need to remain a completely unnamed `Unknown 0x3` solely because public NVIDIA documentation is incomplete.

Separate work remains for density code `0x7`, timing semantics, clock conversion, RAMCFG electrical interpretation and runtime profile selection. Successful organization inference does not validate any of those fields. No product-source change or release was made as part of this research.

## Reproduction and evidence files

The [read-only corpus extractor](../../tools/research/scan_gddr7_organization.py) follows PCI/PCIR, BIT and BIT M pointers and retains every descriptor byte without naming the organization modes. It checks memory-table geometry and computes SHA-256 for each input. It is a research script, not a replacement for the production parser.

```powershell
python tools/research/scan_gddr7_organization.py "PATH/Video-BIOS-Collection/index.json" reports/gddr7-organization/corpus.json
```

The output must not already exist. Raw ROMs are never modified. The generated corpus report includes input paths, catalog IDs, hashes, descriptor offsets, bytes and translation arrays. This run's output is retained in `reports/gddr7-organization/corpus.json`; independent NBR CLI reports for `275774` and `277002` are beside it. These generated files are ignored by Git.

Selected SHA-256 identities:

```text
277002  04f0175c99e92eb1a7eec5a02a85d333d5f94876a2409e18e5504c7b3735cc43
275774  88f2f06faefa44dac431022031f5844e6cc78262c191ecb7466ca0561205b154
276947  2bb7636be2a872db82a70f4740a605d76e48bcb73ea19d2116ef286bd0c41e26
275849  483f33921f26845aeca34ab6f984be16b3a6db9cdd38ceb4bfbeb654f3f32bc4
```

Important limitations: no newly measured active profile; no exact physical board tied to every archived ROM; all corpus records are unverified uploads; full JEDEC text was not obtained; no publicly documented NVIDIA organization enum was located. The conclusion is a high-confidence empirical mapping with those boundaries, not a hardware intervention recommendation.

## Sources

1. NVIDIA. [GeForce RTX 5060 family specifications](https://www.nvidia.com/en-us/geforce/graphics-cards/50-series/rtx-5060-family/). Confirms 8 GB/16 GB GDDR7 and 128-bit interface.
2. Igor Wallossek, Igor'sLAB, April 16, 2025. [RTX 5060 Ti 16 GB teardown, page 3](https://www.igorslab.de/en/nvidia-geforce-rtx-5060-ti-16-gb-in-test-economical-consumption-surprisingly-fast-but-not-with-8gb/3/). Direct board inspection of MSI Gaming Trio; eight memory packages in clamshell.
3. TechPowerUp. [ASUS RTX 5060 Ti 8 GB, ID 277002](https://www.techpowerup.com/vgabios/277002/277002). Archived page metadata captured August 11, 2026; current search result corroborated the identity. ROM inspected locally.
4. TechPowerUp. [ASUS RTX 5060 Ti 16 GB, ID 275774](https://www.techpowerup.com/vgabios/275774/275774). Archived metadata captured August 11, 2026; live fetch timed out. ROM inspected locally.
5. TechPowerUp. [MSI RTX 5060 Ti 8 GB, ID 276947](https://www.techpowerup.com/vgabios/276947/276947). Local archived page metadata and ROM, not a new successful page fetch.
6. TechPowerUp. [MSI RTX 5060 Ti 16 GB, ID 275849](https://www.techpowerup.com/vgabios/275849/275849). Local archived page metadata and ROM, not a new successful page fetch.
7. JEDEC. [JESD239.01 GDDR7 preview, April 2024](https://store.accuristech.com/products/preview/2901627). Indexed contents identify four-channel/two-channel modes and clamshell topology; not a full-text standards validation.
8. SmartDV. [GDDR7 verification IP](https://www.smartdvtech.com/products/gddr7-vip/). Vendor technical description of four x8 channels and two-/four-channel modes; not a NVIDIA VBIOS format specification.
9. GamersNexus. [RTX PRO 6000 Blackwell benchmarks and teardown](https://gamersnexus.net/gpus/nvidia-rtx-pro-6000-blackwell-benchmarks-tear-down-thermals-gaming-llm-acoustic-tests). Written September 25, 2025, adapted from June 24, 2025 video. Direct package-count inspection.
10. NVIDIA. [RTX PRO 6000 Blackwell Workstation Edition datasheet](https://www.nvidia.com/content/dam/en-zz/Solutions/data-center/rtx-pro-6000-blackwell-workstation-edition/workstation-blackwell-rtx-pro-6000-workstation-edition-nvidia-us-3519208-web.pdf). Official 96 GB GDDR7 specification.

The archived per-ROM metadata files are under the collection's `Unverified_Uploads/metadata-audit-full/metadata/` directory, named by TPU ID. No proprietary ROM image is included in this document or added to Git.
