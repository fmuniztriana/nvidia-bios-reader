"""Read-only, sample-specific GDDR7 audit; never execute or flash the input.

Usage: python tools/research/audit_dell_gddr7.py DELL_UPDATE.exe OUTPUT_DIRECTORY
The emitted .bin is an analysis window, NOT an established standalone VBIOS.
Offsets are pinned evidence for one hash, not a generic Blackwell parser.
"""

import argparse
import hashlib
import json
import struct
from pathlib import Path


EXPECTED_SHA256 = "275affdb658b4e8ba13b041dbc118931f614954f1e057bcfc8c8318362ed05ef"
WINDOW_BASE = 0x1683C34
WINDOW_SIZE = 0x200000


def audit(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError("Different package hash: this audit only supports the documented Dell sample")
    rom = data[WINDOW_BASE:WINDOW_BASE + WINDOW_SIZE]
    if len(rom) != WINDOW_SIZE:
        raise ValueError("Truncated analysis window")

    def block(offset, size):
        if offset < 0 or offset + size > len(rom):
            raise ValueError("Out-of-bounds read")
        return rom[offset:offset + size]

    legacy = 0x1200
    pcir = legacy + struct.unpack("<H", block(legacy + 24, 2))[0]
    assert block(legacy, 2) == b"\x55\xaa" and block(pcir, 4) == b"PCIR"
    assert struct.unpack("<HH", block(pcir + 4, 4)) == (0x10DE, 0x2D58)
    assert block(pcir + 20, 1) == b"\0"
    assert block(0x1FF0, 6) == b"\xff\xb8BIT\0"
    token_m = 0x210D
    translation_count = rom[token_m]
    translation_offset, info_offset = (
        legacy + n for n in struct.unpack("<HH", block(token_m + 1, 4))
    )
    translation = list(block(translation_offset, translation_count))
    iv, ih, il, ic = block(info_offset, 4)
    map_offset, table_offset = 0x3985C, 0x3CB91
    mv, mh, mb, me, mx, mc = block(map_offset, 6)
    tv, th, tb, te, tx, tc = block(table_offset, 6)
    map_stride, timing_stride = mb + me * mx, tb + te * tx
    records = [block(table_offset + th + i * timing_stride, timing_stride) for i in range(tc)]
    ranges = []
    for i in range(mc):
        offset = map_offset + mh + i * map_stride
        lo, hi = struct.unpack("<HH", block(offset, 4))
        ranges.append({"offset": hex(offset), "raw_bounds": [lo, hi],
                       "ids": [rom[offset + mb + j * me] for j in range(mx)]})
    profiles = []
    for i in range(ic):
        offset = info_offset + ih + i * il
        raw = block(offset, il)
        descriptor = int.from_bytes(raw[:4], "little")
        ids = [r["ids"][i] for r in ranges] if i < mx else []
        profiles.append({
            "entry": i + 1, "offset": hex(offset), "descriptor": f"0x{descriptor:08X}",
            "record_hex": raw.hex(" "), "type_code": descriptor & 15,
            "vendor_code": (descriptor >> 12) & 15,
            "density_code": (descriptor >> 20) & 15,
            "organization_code": (descriptor >> 24) & 7,
            "physical_codes": [p for p, g in enumerate(translation) if g == i],
            "timing_ids": ids,
            "referenced_zero_prefix_nonzero_records": sorted({
                t for t in ids if t < tc and not any(records[t][:24]) and any(records[t])
            }),
        })
    result = {
        "warning": "Sample-specific structure audit; timing semantics and clocks are NOT validated",
        "package_sha256": digest, "window_base_in_package": hex(WINDOW_BASE),
        "window_size": WINDOW_SIZE, "window_sha256": hashlib.sha256(rom).hexdigest(),
        "translation_offset": hex(translation_offset), "translation": translation,
        "memory_table": {"offset": hex(info_offset), "version": iv, "header": ih,
                         "record_length": il, "count": ic},
        "map": {"offset": hex(map_offset), "header_hex": block(map_offset, mh).hex(" "),
                "version": mv, "base_length": mb, "extended_length": me,
                "extended_count": mx, "count": mc, "stride": map_stride},
        "timings": {"offset": hex(table_offset), "header_hex": block(table_offset, th).hex(" "),
                    "version": tv, "count": tc, "stride": timing_stride},
        "ranges": ranges, "profiles": profiles,
        "timing_records": [{"id": i, "offset": hex(table_offset + th + i * timing_stride),
                            "hex": r.hex(" ")} for i, r in enumerate(records)],
    }
    return rom, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    rom, result = audit(args.package.read_bytes())
    args.output_directory.mkdir(parents=True, exist_ok=True)
    # Exclusive creation prevents overwriting an earlier analysis.
    with (args.output_directory / "dell-5070-analysis-window.bin").open("xb") as output:
        output.write(rom)
    with (args.output_directory / "audit.json").open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
    print("Created read-only research evidence. Do not flash the analysis window.")


if __name__ == "__main__":
    main()
