"""Extract raw GDDR7 descriptors from a local TPU index, without changing ROMs.

This is a research extractor, not a validated decoder of physical topology.
Usage: python scan_gddr7_organization.py INDEX_JSON OUTPUT_JSON
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def descriptors(b):
    def u16(p):
        return struct.unpack_from('<H', b, p)[0]
    legacy = None
    for base in range(0, len(b) - 26, 512):
        if b[base:base+2] != b'\x55\xaa':
            continue
        p = base + u16(base + 24)
        if p + 24 > len(b) or b[p:p+4] != b'PCIR':
            continue
        if u16(p+4) == 0x10DE and b[p+20] == 0:
            legacy = base
            device = u16(p+6)
            length = u16(p+16)*512
            break
    if legacy is None:
        raise ValueError('No NVIDIA legacy image')
    bit = b.find(b'\xff\xb8BIT\x00', legacy, legacy + length)
    if bit < 0:
        raise ValueError('No BIT')
    h, s, n = b[bit+8:bit+11]
    if h < 12 or s < 6:
        raise ValueError('Invalid BIT geometry')
    tokens = {}
    for i in range(n):
        p = bit + h + i*s
        tokens[chr(b[p])] = (b[p+1], u16(p+2), legacy+u16(p+4))
    ver, size, m = tokens['M']
    if ver != 2 or size < 5:
        raise ValueError('Unsupported M')
    translation_offset, table = legacy+u16(m+1), legacy+u16(m+3)
    translation = list(b[translation_offset:translation_offset+b[m]])
    ver, hdr, stride, count = b[table:table+4]
    if hdr < 4 or stride < 4 or table+hdr+count*stride > len(b):
        raise ValueError('Invalid memory table geometry')
    entries = []
    for i in range(count):
        p = table+hdr+i*stride
        raw = b[p:p+stride]
        d = int.from_bytes(raw[:4], 'little')
        entries.append(dict(entry=i+1, offset=p, descriptor=f'{d:08X}',
                            type=d&15, vendor=(d>>12)&15, density=(d>>20)&15,
                            organization=(d>>24)&7, features=d>>27,
                            raw=raw.hex(), physical=[k for k,v in enumerate(translation) if v==i]))
    return dict(device=f'{device:04X}', legacy=legacy, bit=bit, token_m=m,
                table=table, table_version=ver, stride=stride, count=count,
                translation_offset=translation_offset, translation=translation, entries=entries)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('index', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    data = json.loads(args.index.read_text(encoding='utf-8-sig'))
    rows = [e for e in data['entries'] if e.get('memory') == 'GDDR7']
    results = []
    for i, e in enumerate(rows):
        item = {k: e.get(k) for k in ('id','manufacturer','model','version','detailsUrl','verified','sha256')}
        path = (args.index.parent / e['romUrl']).resolve()
        item['path'] = str(path)
        try:
            b = path.read_bytes()
            item['actual_sha256'] = hashlib.sha256(b).hexdigest()
            item['hash_matches_catalog'] = (item['actual_sha256'].lower() == (e.get('sha256') or '').lower()) if e.get('sha256') else None
            item['parsed'] = descriptors(b)
        except (OSError, ValueError, KeyError, IndexError, struct.error) as err:
            item['error'] = str(err)
        results.append(item)
        if (i+1) % 250 == 0:
            print(f'{i+1}/{len(rows)} inspected', flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(dict(index=str(args.index), index_sha256=hashlib.sha256(args.index.read_bytes()).hexdigest(),
                       index_entries=len(data['entries']), candidates=len(rows), results=results), f, indent=2)
    print(f'Finished: {len(results)} catalog records; {sum("parsed" in r for r in results)} parsed')


if __name__ == '__main__':
    main()
