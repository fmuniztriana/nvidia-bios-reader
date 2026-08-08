# Validation Workflow

The project validates reverse-engineered fields against a local ROM corpus
without redistributing the ROM files.

## Sample identity

For every sample, record:

- SHA-256;
- exact byte size;
- public source URL, when available;
- board vendor and model;
- GPU chip and PCI Device ID;
- VBIOS version;
- installed memory vendor, part number, and topology when physically verified;
- active strap value when independently measured.

Do not rely on a filename as sample identity.

## Minimum validation procedure

1. Run the CLI with `--timings` and save the report.
2. Confirm PCI IDs and VBIOS version with an independent parser.
3. Compare Memory Support entries and their ordering.
4. Check descriptor, translation-table, timing-map, and timing-record offsets in
   a hex editor.
5. Confirm that every reported timing ID points inside the declared table.
6. Compare at least two vendors or densities before assigning semantics to a
   changing field.
7. Mark any conclusion that depends on board behavior as observed or inferred.
8. Compare complete timing records, not only decoded CONFIG0..CONFIG5 fields.
9. Record both raw logical pointers and resolved physical file offsets.
10. If runtime tools are used, preserve the exact tool hash and distinguish
    requested geometry from successful memory training.
11. For a claimed RAMCFG alias, confirm that both declared translation bytes
    select the same group. Claim full runtime equivalence only after testing
    both physical selectors on the same board and recording initialization,
    training, and loaded-driver behavior.

## Regression corpus

ROMs belong in a local `corpus/` directory, which is ignored by Git. Generated
reports belong in `reports/`. A future machine-readable manifest will contain
only hashes, public metadata, expected parser results, and optional source
URLs.

Good regression coverage should include:

- a full GDDR6 timing profile;
- a profile with low-range `FF` entries;
- an all-`FF` profile;
- multiple memory vendors in one ROM;
- both 8 Gbit and 16 Gbit descriptors;
- GDDR6X and clamshell-capable profiles;
- malformed pointers and truncated tables using synthetic, redistributable
  fixtures.
- equal decoded fields with different complete raw timing records;
- logical pointers that require an intervening PCI-image adjustment.

## Publication standard

A newly named field should have either a public specification or repeated,
independent evidence. One ROM is sufficient to document a raw pattern, but not
to generalize its meaning across an architecture.
