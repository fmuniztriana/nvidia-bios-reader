# NVIDIA BIOS Reader

NVIDIA BIOS Reader is an open-source, read-only research tool for inspecting
NVIDIA video BIOS memory configuration. It provides a native Windows GUI and a
portable command-line interface without third-party runtime dependencies.

The project began as an investigation into GDDR6 memory-density upgrades and
the low-P-state instability sometimes observed after those modifications. Its
current focus is making memory profiles, strap translation, timing coverage,
and relevant ROM offsets visible and reproducible.

## Why this project exists

The immediate spark was a recurring problem seen in graphics cards modified
with double-density memory. Public projects such as the PauloGomesTeam's
[RTX 3070 16 GB conversion](https://www.youtube.com/watch?v=W6uaUHBNFOU)
showed that an otherwise functional card could flicker, black-screen, or crash
when allowed to leave its high-performance state. Selecting maximum-performance
mode was an effective workaround, which pointed toward a low-power or P-state
transition problem rather than a failure at full memory speed.

The behavior was not universal. One of the cards behind this research, an
RTX 2080 Ti converted from 11 GB to 22 GB in late 2022, operated normally with
dynamic power states. That made "double the density" an incomplete explanation.
The first working theory was that Samsung HC16 devices installed where HC14
parts were expected were receiving timings that were simply too different or
too aggressive at lower clocks.

A separate GTX 1650 8 GB experiment in July 2026 changed that interpretation.
In the affected 16 Gbit memory profile, several low-clock timing-map references
were not merely different: they were `FF`, meaning that no timing record was
referenced for those ranges, while the high-clock ranges remained populated.
This matched the practical symptom unusually well. Forcing a high-performance
state avoided the incomplete part of the map; allowing the card to downclock
reached it.

That observation does not prove that every density-doubling instability has
the same cause. It did provide a concrete and testable question that existing
VBIOS summaries did not answer: for every vendor and density profile declared
by a ROM, are all operating ranges actually backed by timing records? NVIDIA
BIOS Reader was created to make that answer visible.

> [!IMPORTANT]
> This is an experimental reverse-engineering project. Several decoded fields
> are based on repeated observations across real ROMs rather than a complete
> public NVIDIA specification. Inferred values are identified as such.

## Current capabilities

- identify the GPU chip, PCI Device ID, PCI Vendor ID, and VBIOS version;
- enumerate every Memory Information entry declared by the ROM;
- report memory type, vendor, per-device density, and organization code;
- map logical memory entries to physical strap selector values;
- locate memory timing-map and timing-record tables;
- classify each profile as `FULL`, `PARTIAL`, `EMPTY (all FF)`, or invalid;
- show timing IDs and exact offsets for descriptors, map bytes, and records;
- decode the currently known CONFIG0 through CONFIG5 controller fields;
- export a detailed plain-text report;
- operate entirely read-only: the input ROM is never modified.

VBIOS editing and flashing are intentionally outside the current scope.

## Supported scope

The parser has been exercised against real TU106, TU116, and GA102 ROMs. The
underlying layouts are related across Turing, Ampere, and Ada, but untested
chips must be treated as experimental until validated against multiple ROMs
and independent tools.

| Area | Current status |
| --- | --- |
| TU106 and TU116 memory profiles | Validated on multiple ROMs |
| TU106 and TU116 GDDR6 timing maps | Validated on multiple ROMs |
| GA102 GDDR6X memory profiles | Initial validation |
| Other Turing, Ampere, and Ada chips | Experimental |
| VBIOS editing or flashing | Not supported |

See [Format and confidence notes](docs/format-notes.md) for the distinction
between directly parsed, observed, and inferred fields.

For a byte-by-byte walkthrough using only a hex editor, see
[Manual memory-table inspection](docs/manual-inspection.md).

## Windows graphical interface

Open `nvidia-bios-reader.exe`, choose **Open VBIOS**, or drag a ROM onto the
window. Select a memory entry to inspect its timing ranges and select a timing
range to view the decoded CONFIG fields. **Save Report** writes the complete
analysis to a text file.

The GUI uses native Win32 controls. It does not require Qt, .NET, or additional
DLLs.

## Command-line interface

Analyze a ROM:

```text
nvidia-bios-reader-cli card.rom
```

Include decoded timing fields:

```text
nvidia-bios-reader-cli card.rom --timings
```

Save a report:

```text
nvidia-bios-reader-cli card.rom --timings --output card-report.txt
```

Show all options:

```text
nvidia-bios-reader-cli --help
```

## Building from source

Requirements:

- CMake 3.20 or newer;
- a C++20 compiler;
- Windows SDK when building the GUI.

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Windows with MSVC, the release executables use the static C++ runtime and
can be distributed as standalone files. On non-Windows systems only the CLI is
built. Set `NVBR_BUILD_GUI=OFF` to disable the GUI explicitly.

## Interpretation rules

- Density describes one memory device. It does not determine total VRAM,
  device count, active strap, or the physical board population by itself.
- An entry in Memory Support is a profile available to the firmware; it is not
  proof that the shipping board uses that profile.
- `FULL` means every used frequency range references a present non-zero timing
  record for that memory group.
- `PARTIAL` means at least one used range has a timing and at least one is
  `FF`.
- `EMPTY (all FF)` means the descriptor exists but every used range is `FF`.
- A `0-0` map slot is treated as unused and excluded from coverage.
- Decoded CONFIG values are controller fields or cycle counts, not
  nanoseconds.
- The displayed-clock conversion `raw / 4` is an observed convention and is
  explicitly marked as inferred.
- Organization labels such as clamshell describe the decoded profile and do
  not independently prove the physical topology of a particular board.

## Repository layout

```text
include/                 Public data model and parser API
src/nvbios_reader.cpp    VBIOS parser and text report generation
src/cli.cpp              Portable command-line interface
src/gui.cpp              Native Windows interface
docs/                    Architecture, format, and validation notes
.github/                 Continuous integration and contribution templates
```

ROM images are deliberately excluded from the repository. Research findings
should identify samples by SHA-256 and public source URL where redistribution
is not permitted. See [Validation workflow](docs/validation.md).

## Contributing

Reproducible findings, additional ROM validation, documentation corrections,
and parser improvements are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md)
before opening an issue or pull request.

## Research references

- [Paulo Gomes: RTX 3070 modified to 16 GB](https://www.youtube.com/watch?v=W6uaUHBNFOU)
- [Paulo Gomes: GTX 1650 modified to 8 GB](https://www.youtube.com/watch?v=IyJ3fYF6sJw)
- [VideoCardz: RTX 3070 16 GB and the high-performance-mode workaround](https://videocardz.com/newz/modded-geforce-rtx-3070-with-16gb-memory-gets-major-1-low-fps-boost)
- [Igor'sLAB: technical review of the GTX 1650 8 GB project](https://www.igorslab.de/en/gtx-1650-8-gb-upgrade/)
- [NVIDIA open-gpu-doc: Virtual BIOS](https://github.com/NVIDIA/open-gpu-doc/tree/master/virtual-bios)
- [envytools nvbios](https://github.com/envytools/envytools/tree/master/nvbios)
- [ImHex](https://github.com/WerWolv/ImHex)

NVIDIA BIOS Reader is an independent project and is not affiliated with or
endorsed by NVIDIA Corporation or TechPowerUp.

## License

Released under the [MIT License](LICENSE).
