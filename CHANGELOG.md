# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project intends to follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned

- machine-readable JSON reports;
- regression tests using synthetic fixtures and privately held ROM hashes;
- broader Turing, Ampere, and Ada validation;
- additional read-only VBIOS tables.

## [0.4.0-beta.1] - 2026-08-08

Pre-release notes are available in
[`docs/releases/v0.4.0-beta.1.md`](docs/releases/v0.4.0-beta.1.md).

### Added

- complete raw timing-record extraction using the table-declared stride;
- CRC32 fingerprints for grouping byte-identical timing records;
- `--compare-profiles A B` CLI mode with per-range and per-byte differences;
- native GUI profile selection and complete per-byte comparison view;
- GUI and CLI RAMCFG map with all 16 standard codes, exact translation-byte
  offsets, target profiles, aliases, and outside-table states;
- explicit detection of equal decoded CONFIG0..CONFIG5 fields whose complete
  records still differ;
- pointer map for memory, training, script, TMRS, performance, clock, tweak,
  and power-sensor tables with raw and resolved offsets;
- GUI display of the complete selected timing record;
- detailed raw timing-record inventory in exported timing reports;
- beginner-friendly Windows and Linux compilation guide;
- reproducible GA104 Samsung 8/16 Gbit static/runtime case study.

### Changed

- clarified that `FULL` describes timing-map coverage only and cannot prove
  boot, training, P-state, or hardware stability;
- versioned the research line as `0.4.0-beta.1`;
- documented post-legacy pointer adjustment when an intervening PCI image is
  present;
- preserved bytes after timing-record offset `+0x17` as explicitly unnamed
  evidence instead of inferring field semantics.

### Experimental findings

- observed a persistent `+0x33 A2/A3` difference between one GA104 Samsung
  8/16 Gbit profile pair despite equal decoded fields in six used ranges;
- correlated density selection with geometry, MRS, electrical, command-map,
  and calibration changes in a controlled physical-strap experiment;
- documented why NVMT `ts/ts2` value `0x555` and `gddrinfo` output must not be
  treated as universal GA104 GDDR6 failure indicators.

## [0.3.0] - 2026-08-08

Release notes are available in
[`docs/releases/v0.3.0.md`](docs/releases/v0.3.0.md).

### Added

- physical RAMCFG decoding with five-bit values and STRAP2/1/0 L/M/H levels;
- explicit translation-table mappings in exported CLI and GUI reports;
- separate declared-record, described-profile, and referenced-profile counts;
- contextual GUI tooltips for RAMCFG notation, profile counts, timing-status
  classifications, clock ranges, and decoded CONFIG fields;
- focused terminology tooltips for entry/group numbering, per-device density,
  memory organization, raw descriptors, and ROM map/record offsets;
- separate NVIDIA/Afterburner MCLK ranges from inferred memory-device clocks,
  using the GDDR6 and GDDR6X-specific conversion factors;
- manual documentation for the 16-code multilevel RAMCFG convention.

### Changed

- clarified the difference between electrically possible strap codes and the
  physical codes declared by a particular VBIOS translation table;
- shortened physical strap labels in the GUI while retaining full wording in
  profile details and exported reports;
- marked development builds as `0.3.0-dev` to distinguish them from v0.2.0;
- corrected the former universal `raw / 4` clock conversion, which overstated
  the GPU-Z-style device clock for GDDR6X by two times;
- batch-positioned child controls during live resizing, removed full-window
  erase on every pixel, and throttled only column/tooltip geometry updates.

## [0.2.0] - 2026-08-06

### Added

- native Windows graphical interface;
- portable command-line interface;
- GPU chip, PCI ID, and VBIOS version reporting;
- Memory Information and physical strap translation decoding;
- memory timing-map coverage classification;
- CONFIG0 through CONFIG5 timing-field decoding;
- detailed report export with source offsets;
- CMake build, install rules, and basic CLI test;
- repository documentation and GitHub continuous integration;
- project-origin notes and public references for density-mod instability.
- author, citation, release, and Windows executable version metadata.
- discreet clickable author credit in the lower-right status area;
- complete About dialog with version, author, GitHub links, license, and
  NVIDIA independence notice.

### Changed

- replaced resize-sensitive group-box controls with stable section headings;
- coalesced GUI repaint requests and avoided redundant column resizing.
