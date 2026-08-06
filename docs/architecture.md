# Architecture

NVIDIA BIOS Reader deliberately keeps a small separation between parsing,
presentation, and operating-system code.

## Components

### `nvbr_core`

`src/nvbios_reader.cpp` owns all binary parsing and report generation. It reads
the complete input into memory, wraps it in a bounds-checked byte view, and
returns the public `nvbr::Document` model declared in
`include/nvbios_reader.hpp`.

The parser currently follows this sequence:

1. enumerate PCI expansion-ROM images;
2. choose the NVIDIA legacy image;
3. locate and parse the NVIDIA BIT token table;
4. read the information token for the VBIOS version and GPU code;
5. read the memory token, Memory Information table, and strap translation;
6. locate the performance timing map and timing-record table;
7. associate every memory profile and range with its timing record;
8. generate concise and detailed text reports.

No parser function writes to the input file.

### Command-line interface

`src/cli.cpp` handles arguments, calls `nvbr::inspect_vbios`, prints the chosen
report, and optionally saves it. It contains no format-specific parsing.

### Windows graphical interface

`src/gui.cpp` displays the same `nvbr::Document` data through native Win32
controls. The GUI contains no separate VBIOS decoder and therefore should not
produce results that differ from the CLI.

## Error handling

Every byte read passes through the bounds-checked `Bytes` view. Invalid table
geometry raises a parsing error with an offset where possible. Optional timing
tables may be reported as unavailable without preventing memory-profile
inspection.

## Design constraints

- read-only input;
- no external runtime dependencies;
- C++20 and standard-library-first implementation;
- parser remains portable even when the GUI is Windows-specific;
- exact source offsets remain visible for independent verification;
- incomplete knowledge is represented as unknown or inferred, never silently
  promoted to a confirmed specification.

As format coverage grows, the parser can be split by table family without
changing the public `Document` model. The present three-source layout is kept
until that split provides a concrete maintenance benefit.
