#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify original PAL Gadgebot HUD resources; emit metadata, never disc assets."""
import argparse
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_pal_tables import Disc, SECTOR, ISO_SHA256, sections, static_range, occurrences, sha, word


def audit(path):
    config = json.loads((Path(__file__).resolve().parents[1] / 'compatibility.json').read_text())
    with Disc(path) as disc:
        identity = disc.identity()
        if not identity['matches_official_openrac_0_10_0_boot'] or disc.full_digest() != ISO_SHA256:
            raise ValueError('Expected the verified PAL v2.00 disc')
        toc = disc.read(1500 * SECTOR, 0x2960)
        levels = [disc.level(toc, i) for i in range(19)]
        loaded = [sections(disc.lump(level, 0)) for level in levels]
        meta = disc.lump(levels[6], 0x20)
        from verify_pal_tables import wad_decompress
        pixels = wad_decompress(disc.lump(levels[6], 0x30))
    table_results = []
    for index, item in enumerate(config['native_tables'][-2:]):
        signature = static_range(loaded[0], int(item['canonical'],16), item['size'])
        assert sha(signature) == item['sha256']
        for level, (image, address) in enumerate(zip(loaded,item['addresses'])):
            at = int(address,16)
            assert static_range(image,at,item['size']) == signature
            if index == 0:
                assert occurrences(image,signature) == [at]
            else:
                # Four integers also occur in other navigation tables. Anchor
                # this one to the unique 4-command record table, not a scan hit.
                assert at == int(config['native_tables'][-2]['addresses'][level],16) + 0x98
        table_results.append(item)
    defs, refs, palettes, images = (word(meta,i) for i in (4,8,12,16))
    row = next(defs+i*8 for i in range(2048) if word(meta,defs+i*8)&65535 == 0x7553)
    assert word(meta,row)>>16 == 8
    first = word(meta,row+4)&65535
    pairs = [word(meta,refs+(first+i)*4) for i in range(8)]
    palette_offsets = [word(meta,palettes+(pair&65535)*8)&0x7fffffff for pair in pairs]
    assert len(set(palette_offsets)) == 1
    start = palette_offsets[0]
    bundle = pixels[start:start+1024]
    for pair in pairs:
        at = images+(pair>>16)*8
        assert word(meta,at+4)>>16 == 0x0505
        offset=word(meta,at)&0x7fffffff
        bundle += pixels[offset:offset+1024]
    assert len(bundle)==9216 and sha(bundle)==config['hud']['icon_sha256']
    return {'format':1,'region':'pal','disc_revision':'2.00','openrac_commit':config['openrac_commit'],
            'disc':identity,'tables':table_results,'icon':{'id':0x7553,'frames':8,'size':[32,32],
            'bytes':len(bundle),'sha256':sha(bundle)},'passed':True,
            'coverage':'Static disc identity only. Native selection, drawing, sound and lifecycle need separate gameplay tests.'}


if __name__ == '__main__':
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('iso',type=Path)
    p.add_argument('--json',type=Path,required=True)
    args=p.parse_args()
    result=audit(args.iso)
    args.json.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PAL HUD audit passed: 19 overlays, original eight-frame command icon')
