"""Regression checks using a locally generated professional-profile corpus.

Usage: python -B test_gddr7_reader.py CLI_EXE PROFILES_JSON
No ROM is modified. Test copies exist only in a temporary directory.
"""
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def run(exe, *args):
    return subprocess.run([str(exe), *map(str, args)], capture_output=True,
                          encoding='utf-8', errors='replace', timeout=30)


def main():
    exe, corpus = Path(sys.argv[1]).resolve(), Path(sys.argv[2])
    samples = json.loads(corpus.read_text(encoding='utf-8'))['results']
    for row in samples:
        path = Path(row['path'])
        assert hashlib.sha256(path.read_bytes()).hexdigest() == row['actual_sha256']
        result = run(exe, path, '--timings')
        assert result.returncode == 0, (row['id'], result.stderr)
        text = result.stdout
        assert 'GDDR7 (experimental)' in text and 'units unvalidated' in text
        assert not re.search(r'\bRC=\d|Range \d+.*MCLK', text), row['id']
        assert 'Named timing decoding unavailable' in text
        expected = any(p['zero_prefix_nonzero_tail'] for p in row['analysis']['profiles'])
        assert ('Zero 24-byte prefix / nonzero tail' in text) == expected, row['id']
        comp = run(exe, path, '--compare-profiles', '1', '2')
        assert comp.returncode == 0 and 'UNAVAILABLE (experimental GDDR7)' in comp.stdout
        assert 'Decoded CONFIG0..CONFIG5: IDENTICAL' not in comp.stdout
        assert 'Decoded CONFIG0..CONFIG5 equal: N/A' in comp.stdout
    # Unicode paths and malformed geometry without altering the source input.
    original = Path(samples[0]['path']).read_bytes()
    with tempfile.TemporaryDirectory(prefix='nbr-gddr7-') as temp:
        path = Path(temp) / '\u6d4b\u8bd5-\u043f\u0430\u043c\u044f\u0442\u044c.rom'
        path.write_bytes(original)
        assert run(exe, path).returncode == 0
        # Memory information pointer is a legacy-relative u16 at BIT M + 3.
        m = samples[0]['analysis']['tokens']['M']['offset']
        # Infer the base from the known BIT offset and its location relative to PCI image.
        candidates = [p for p in range(0, len(original), 512) if original[p:p+2] == b'\x55\xaa']
        base = max(p for p in candidates if p <= m)
        table = base + int.from_bytes(original[m+3:m+5], 'little')
        mutated = bytearray(original)
        assert mutated[table:table+3] == bytes([0x10, 7, 22])
        mutated[table] = 0x99
        path.write_bytes(mutated)
        result = run(exe, path)
        assert result.returncode != 0 and 'Unsupported experimental GDDR7' in result.stderr
        path.write_bytes(original[:64])
        assert run(exe, path).returncode != 0
    print(f'PASS: {len(samples)} GDDR7 reports and comparisons; Unicode, unsupported layout, truncated input.')


if __name__ == '__main__':
    main()
