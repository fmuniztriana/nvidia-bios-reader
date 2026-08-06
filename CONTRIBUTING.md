# Contributing

Thank you for helping improve NVIDIA BIOS Reader. The project values
reproducible evidence and clear separation between confirmed structure,
repeated observation, and hypothesis.

## Before reporting a finding

1. Record the VBIOS SHA-256 hash and file size.
2. Record the board model, GPU chip, Device ID, memory part number, and active
   physical strap when known.
3. Compare the result with at least one independent tool when possible.
4. Remove serial numbers, local paths, and other private information.
5. Do not attach a ROM unless you have permission to redistribute it.

Reports based on a public ROM should link to its original public page instead
of adding the binary to the repository.

## Building and testing

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The project is warning-clean at MSVC `/W4` and GCC/Clang
`-Wall -Wextra -Wpedantic`. New code should remain warning-clean.

## Pull requests

- keep changes focused and explain the evidence behind format assumptions;
- preserve read-only behavior;
- bounds-check every ROM access;
- label inferred values in code, reports, and documentation;
- update the changelog for user-visible changes;
- avoid unrelated formatting changes;
- never commit ROM images or proprietary diagnostic packages.

## Commit messages

Use short imperative summaries, for example:

```text
Add GA104 memory-table validation
Fix timing-map bounds check
Document organization-code confidence
```

## Coding style

The repository includes `.editorconfig` and `.clang-format`. Use C++20 and
prefer standard-library facilities over new dependencies. The native GUI is
kept separate from the parser so the core remains portable and testable.
