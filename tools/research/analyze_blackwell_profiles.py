"""Read-only raw timing-map comparison for an existing GDDR7 corpus scan.

Usage: python -B analyze_blackwell_profiles.py CORPUS_JSON OUTPUT_JSON
No GDDR7 timing bitfields, clock conversions, or active profiles are inferred.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def analyze(b, parsed):
    def read(p, n):
        if p < 0 or p+n > len(b):
            raise ValueError('Out-of-bounds read')
        return b[p:p+n]
    def u16(p):
        return struct.unpack('<H', read(p, 2))[0]
    def u32(p):
        return struct.unpack('<I', read(p, 4))[0]
    base = parsed['legacy']
    pcir = base+u16(base+24)
    length = u16(pcir+16)*512
    efi, efi_len = base+length, 0
    if read(efi, 2) == b'\x55\xaa':
        ep = efi+u16(efi+24)
        if read(ep, 4) == b'PCIR' and read(ep+20, 1) == b'\x03':
            efi_len = u16(ep+16)*512
    def resolve(p):
        return base+p+(efi_len if p > length else 0)
    bit = parsed['bit']
    hdr, stride, count = read(bit+8, 3)
    tokens = {}
    for i in range(count):
        p = bit+hdr+i*stride
        tokens[chr(b[p])] = dict(version=b[p+1], length=u16(p+2), offset=base+u16(p+4))
    perf = tokens['P']
    if perf['length'] < 12:
        raise ValueError('Short BIT P')
    mo, to = resolve(u32(perf['offset']+4)), resolve(u32(perf['offset']+8))
    mv, mh, mb, me, mx, mc = read(mo, 6)
    tv, th, tb, te, tx, tc = read(to, 6)
    if mv != 0x11 or mh < 6 or mb < 4 or me < 1 or th < 6:
        raise ValueError('Unexpected timing geometry')
    ms, ts = mb+me*mx, tb+te*tx
    if ts < 24:
        raise ValueError('Short record')
    records = [read(to+th+i*ts, ts) for i in range(tc)]
    ranges = []
    for i in range(mc):
        p = mo+mh+i*ms
        ranges.append(dict(offset=p, low=u16(p), high=u16(p+2),
                           ids=[read(p+mb+g*me,1)[0] for g in range(mx)]))
    profiles = []
    for entry in parsed['entries']:
        if entry['type'] != 13:
            continue
        g = entry['entry']-1
        selected = [r['ids'][g] if g < len(r['ids']) else None for r in ranges]
        used = [t for t,r in zip(selected,ranges) if (r['low'],r['high']) != (0,0)]
        ff = sum(t == 255 for t in used)
        invalid = sum(t is None or (t != 255 and (t >= tc or not any(records[t]))) for t in used)
        present = sum(t is not None and t != 255 and t < tc and any(records[t]) for t in used)
        status = 'INVALID' if invalid else 'FULL' if present and not ff else 'EMPTY' if ff and not present else 'PARTIAL'
        profiles.append(dict(entry=entry['entry'], descriptor=entry['descriptor'],
                             density=entry['density'], organization=entry['organization'], vendor=entry['vendor'],
                             physical=entry['physical'], ids=selected, coverage=status,
                             zero_prefix_nonzero_tail=sorted({t for t in used if t is not None and t < tc
                                 and not any(records[t][:24]) and any(records[t])})))
    m = tokens['M']
    pointers = []
    for relative in range(5, min(m['length'],41)-3, 4):
        raw = u32(m['offset']+relative)
        if raw:
            p = resolve(raw)
            pointers.append(dict(token_relative=relative, raw=raw, resolved=p,
                                 head=read(p, min(32,len(b)-p)).hex() if 0 <= p < len(b) else None))
    return dict(tokens=tokens, map_offset=mo, map_header=read(mo,mh).hex(), map_stride=ms,
                table_offset=to, table_version=tv, timing_stride=ts, record_count=tc,
                ranges=ranges, profiles=profiles, memory_pointers=pointers,
                records=[dict(id=i, offset=to+th+i*ts, sha256=hashlib.sha256(r).hexdigest(),
                              prefix24=r[:24].hex(), raw=r.hex()) for i,r in enumerate(records)])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('corpus', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    source = json.loads(args.corpus.read_text(encoding='utf-8'))
    # Device IDs present in the professional-labelled sample; include consumer controls.
    ids = {'2BB1','2BB3','2BB4','2BB5','2C31','2C33','2C34','2C38','2C39','2D30','2D39','2DB8','2DB9'}
    chosen = [r for r in source['results'] if r['parsed']['device'] in ids or r['id'] in {'277002','275774'}]
    result = []
    for r in chosen:
        item = {k:r[k] for k in ('id','model','version','actual_sha256','path')}
        item['device'] = r['parsed']['device']
        try:
            b = Path(r['path']).read_bytes()
            if hashlib.sha256(b).hexdigest() != r['actual_sha256']:
                raise ValueError('Input changed since corpus scan')
            item['analysis'] = analyze(b, r['parsed'])
        except (ValueError, KeyError, IndexError, struct.error) as err:
            item['error'] = str(err)
        result.append(item)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    by_id = {r['id']: r['analysis'] for r in result if 'analysis' in r}
    comparisons = []
    for left, right in [('277002', '275774'), ('282929', '283595'),
                        ('283591', '278794'), ('283591', '283527')]:
        if left not in by_id or right not in by_id:
            continue
        a, b = by_id[left], by_id[right]
        detail = []
        for p in a['profiles']:
            q = next((q for q in b['profiles'] if q['entry'] == p['entry']), None)
            if q is None:
                continue
            rows = []
            for i, (x, y) in enumerate(zip(p['ids'], q['ids'])):
                if x is None or y is None or x == 255 or y == 255:
                    continue
                if x >= a['record_count'] or y >= b['record_count']:
                    continue
                ar, br = bytes.fromhex(a['records'][x]['raw']), bytes.fromhex(b['records'][y]['raw'])
                rows.append(dict(range_index=i, same_bounds=all(a['ranges'][i][k] == b['ranges'][i][k] for k in ('low','high')),
                                 left_id=x, right_id=y, equal=ar == br,
                                 changed_bytes=[dict(offset=k,left=u,right=v) for k,(u,v) in enumerate(zip(ar,br)) if u != v]))
            detail.append(dict(entry=p['entry'], ranges=rows))
        comparisons.append(dict(left=left,right=right,profiles=detail))
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(dict(source_corpus=str(args.corpus), results=result, comparisons=comparisons), f, indent=2)
    print(f'{len(result)} files inspected; {sum("error" in r for r in result)} errors')


if __name__ == '__main__':
    main()
