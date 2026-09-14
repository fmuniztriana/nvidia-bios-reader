# Experimental GDDR7 implementation

Date: 2026-09-14. Local preview: `0.4.0-internal.3-gddr7`.

This implementation follows the [professional Blackwell audit](blackwell-professional-memory-profiles.md).
That audit's statement that no production code changed describes the research stage;
this subsequent change enables experimental structural reading in the shared GUI/CLI core.

## Supported and deliberately unavailable

- Type `0xD` is labelled `GDDR7 (experimental)`.
- The observed memory-information layout is gated to version 0x10, header 7,
  record length 22. Other GDDR7 descriptor layouts fail explicitly.
- Vendor, descriptors, offsets, declared translations and raw timing records remain visible.
- Organization 2 is labelled `2CH x8 / x16 clamshell (inferred)` and 3 is
  `4CH x8 / x32 (inferred)`. This describes aggregate device interfaces,
  not detected PCB population or the active profile.
- Density 7 is `24 Gbit (inferred)` only for GDDR7; legacy density handling is unchanged.
- Raw bounds are not labelled MHz. The GUI shows `Unvalidated (GDDR7)` instead
  of a converted device clock. No P-state names are assigned.
- Named CONFIG0–CONFIG5 decoding is disabled for GDDR7 in GUI, CLI and comparisons.
- Coverage remains structural only. Zero-prefix/nonzero-tail records are flagged
  in details, not declared corrupt or automatically counted as missing records.
- Profile comparisons retain complete raw bytes, CRC32 identifiers and byte differences.
  Decoded equality is explicitly unavailable, never inferred from two empty strings.
- Unknown Blackwell chip-name mappings remain unknown; memory recognition does not
  imply complete GPU identification or support for every Blackwell firmware layout.

The corrected local preview displays VBIOS versions in the GUI and reports
(`NVBR_HIDE_VBIOS_VERSION=OFF`); hiding them was a temporary internal experiment.
Unicode filenames and the user's icon are preserved. No release was published.

## Validation

Release GUI and CLI compiled with MSVC. CLI help/version CTest checks passed.
All 59 GDDR7 samples from the audit passed report and raw comparison regression
checks. Tests verified that named timings and converted range clocks were absent,
and that zero-prefix warnings matched the independent research extraction.
Unicode filenames, unsupported descriptor version and truncated input were tested.

Four legacy samples (TU116, GA102, AD103 and MSI RTX 3070) were compared with the
previous internal executable: decoded timing lines, raw range lines, coverage,
record identifiers and raw inventory bytes remained equal.

Reproduce the corpus tests, using local files rather than redistributed ROMs:

```sh
python -B tools/research/test_gddr7_reader.py path/to/nvidia-bios-reader-cli reports/blackwell-pro/profiles-with-comparisons.json
```

GUI rendering and usability still require user preview; successful compilation and
shared-core tests do not establish visual correctness or runtime memory stability.
