#!/usr/bin/env python3
"""Append two original Gemlik asteroid variants to a NEW private asset pack.

No game routine, placement, damage, field or seed is copied. Source models,
texels, UV repetition and colours are preserved; only a documented common
translation/uniform scale changes each model's positions. No AI or LOD switch.
"""
from __future__ import annotations
import argparse, collections, json, math, shutil, struct
from pathlib import Path
from PIL import Image
from asset_formats import digest
from batch_assets import GUTTER, REPEAT, check_bilinear, make_atlas
from station_assets import Inputs, read_model

VARIANTS = ((212, 'asteroid_0'), (1412, 'asteroid_1'))
METADATA = ('assets.json', 'assets.tsv', 'provenance.json', 'inventory.json')

def checked_file(root, relative):
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError('Asset path escapes its source root')
    return path

def components(parts):
    """Weld coincident positions across UV/material seams, retain all surfaces.

    The original rocks have a core plus separately meshed crater patches.
    Topological islands alone therefore do not mean independent flying rocks.
    """
    ids, points, parent, triangles = {}, [], [], []
    def node(p):
        key = tuple(round(x * 100000) for x in p)
        if key not in ids:
            ids[key] = len(points); points.append(p); parent.append(len(parent))
        return ids[key]
    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]; i = parent[i]
        return i
    for vertices, indices, *_ in parts:
        mapping = [node(v[0]) for v in vertices]
        for i in range(0, len(indices), 3):
            tri = [mapping[k] for k in indices[i:i + 3]]; triangles.append(tri)
            for k in tri[1:]: parent[find(k)] = find(tri[0])
    groups = collections.defaultdict(list)
    for tri in triangles: groups[find(tri[0])].append(tri)
    result = []
    for tris in sorted(groups.values(), key=len, reverse=True):
        ps = [points[i] for tri in tris for i in tri]
        lo = [min(p[k] for p in ps) for k in range(3)]
        hi = [max(p[k] for p in ps) for k in range(3)]
        result.append({'triangles': len(tris), 'bounds': [lo, hi]})
    return result

def normalization(parts):
    positions = [v[0] for part in parts for v in part[0]]
    if not positions or not all(math.isfinite(x) for p in positions for x in p):
        raise ValueError('Empty/non-finite asteroid positions')
    lo = [min(p[k] for p in positions) for k in range(3)]
    hi = [max(p[k] for p in positions) for k in range(3)]
    centre = [(lo[k] + hi[k]) / 2 for k in range(3)]
    radius = max(math.dist(p, centre) for p in positions)
    if radius < 1e-8 or not math.isfinite(radius): raise ValueError('Degenerate asteroid')
    # Leave a one-part-per-million margin for float32 rounding. The exported
    # radius never exceeds one, so the core's radius is conservative.
    return {'bounds': [lo, hi], 'centre': centre, 'radius': radius,
            'divisor': radius * 1.000001, 'float32_safety_margin': 1e-6}

def source_models(levels, region):
    inputs = Inputs(); root = levels.resolve() / 'level_13'
    manifest = json.loads(inputs.read(root / 'manifest.json'))
    if (manifest.get('game'), manifest.get('version'), manifest.get('level')) != ('rac1', region, 13):
        raise ValueError('Gemlik source region/game/level mismatch')
    placements = json.loads(inputs.read(root / 'placements.json'))
    cache, models = {}, []
    for class_id, name in VARIANTS:
        entry = manifest['classes']['moby'][str(class_id)]
        if entry.get('name') != 'asteroid': raise ValueError('Expected the original asteroid class')
        path = checked_file(root, entry['mesh'])
        parts = read_model(path, inputs, cache)
        for vertices, indices, key, image, factor, _ in parts:
            if key[3:6] != (REPEAT, REPEAT, 'OPAQUE') or image.getextrema()[3] != (255, 255):
                raise ValueError('Asteroid is not original opaque REPEAT material')
            if any(v[4] != -1 for v in vertices): raise ValueError('Unexpected native light slots')
            if not all(math.isfinite(x) for v in vertices for field in v[:4] for x in field):
                raise ValueError('Non-finite native attribute')
        models.append({'id': name, 'class': class_id, 'path': str(path), 'parts': parts,
            'placed_count': sum(p['class'] == class_id for p in placements['mobys']),
            'native_class_scale': entry['scale'], 'normalization': normalization(parts),
            'topological_components': components(parts)})
    return models, inputs

def variant_buffers(model, rects):
    vertices, indices, sources = bytearray(), bytearray(), []
    centre = model['normalization']['centre']; divisor = model['normalization']['divisor']
    vb = ib = 0
    for material, (vv, ii, key, image, factor, source) in enumerate(model['parts']):
        tile = key[0]; rect = rects[tile]
        for p, normal, uv, colour, _ in vv:
            pos = tuple((p[k] - centre[k]) / divisor for k in range(3))
            color = tuple(colour[k] * factor[k] for k in range(4))
            vertices.extend(struct.pack('<16f', *pos, *normal, *uv, *color, *rect))
        indices.extend(struct.pack('<' + 'I' * len(ii), *(i + vb for i in ii)))
        sources.append({'material': material, 'vertex_base': vb, 'index_base': ib,
            'vertex_count': len(vv), 'index_count': len(ii), 'tile': tile,
            'source_texture': source, 'rect': rect})
        vb += len(vv); ib += len(ii)
    return bytes(vertices), bytes(indices), sources

def inventory(root):
    return {p.name: {'bytes': p.stat().st_size, 'sha256': digest(p.read_bytes())}
            for p in sorted(root.iterdir()) if p.is_file() and p.name != 'inventory.json'}

def validate_inventory(root, entries):
    for name, expected in entries.items():
        raw = checked_file(root, name).read_bytes()
        if (len(raw), digest(raw)) != (expected['bytes'], expected['sha256']):
            raise ValueError(f'Asset inventory mismatch: {name}')

def build(base, levels, region, out):
    base, levels, out = base.resolve(), levels.resolve(), out.resolve()
    if out.exists() or out.is_relative_to(base) or base.is_relative_to(out):
        raise ValueError('Output must be a new directory separate from the base pack')
    old = json.loads((base / 'assets.json').read_text())
    provenance = json.loads((base / 'provenance.json').read_text())
    if not old.get('full_galaxy') or old.get('asteroids_extension_format'):
        raise ValueError('Expected an unextended full-galaxy base pack')
    if old.get('region', provenance.get('disc')) != region:
        raise ValueError('Base pack and native source must have the same region')
    old_inventory = json.loads((base / 'inventory.json').read_text())
    validate_inventory(base, old_inventory)
    base_identity = {name: digest((base / name).read_bytes()) for name in METADATA}
    models, inputs = source_models(levels, region)
    tiles = {}
    for model in models:
        for _, _, key, image, *_ in model['parts']:
            tiles.setdefault(key[0], {'key': key[0], 'width': image.width,
                'height': image.height, 'rgba': image.tobytes()})
    atlas, rects, pixels = make_atlas(list(tiles.values()), REPEAT, REPEAT, 2048)
    error = max(check_bilinear(Image.frombytes('RGBA', (t['width'], t['height']), t['rgba']),
        atlas, rects[t['key']], REPEAT, REPEAT) for t in tiles.values())
    # Every old file is copied without alteration. Metadata has exact backup
    # copies because the current manifest/inventory must describe the extension.
    shutil.copytree(base, out)
    for name in METADATA: shutil.copyfile(base / name, out / ('base-0.3.3.' + name))
    atlas.save(out / 'atlas_asteroids.png')
    (out / 'atlas_asteroids.rgba.bin').write_bytes(atlas.tobytes())
    entries, records = [], []
    for model in models:
        name = model['id']; vv, ii, sources = variant_buffers(model, rects)
        (out / (name + '.vertices.bin')).write_bytes(vv)
        (out / (name + '.indices.bin')).write_bytes(ii)
        radius = max(math.sqrt(sum(x*x for x in v[:3])) for v in struct.iter_unpack('<16f', vv))
        if radius > 1: raise ValueError('Float32 asteroid exceeds its collision bound')
        entries.append({'id': name, 'vertices': name + '.vertices.bin',
            'indices': name + '.indices.bin', 'rgba': 'atlas_asteroids.rgba.bin',
            'width': atlas.width, 'height': atlas.height, 'vertex_stride': 64,
            'vertex_count': len(vv) // 64, 'index_count': len(ii) // 4,
            'assembly': name, 'native_level': 13, 'native_class': model['class'],
            'wrap_s': REPEAT, 'wrap_t': REPEAT, 'alpha_mode': 'OPAQUE',
            'alpha_cutoff': .5, 'double_sided': True, 'atlas': True,
            'atlas_gutter': GUTTER, 'local_centre': [0, 0, 0], 'local_bound_radius': 1,
            'max_vertex_radius': radius, 'sources': sources})
        records.append({k: v for k, v in model.items() if k != 'parts'})
    manifest = {**old, 'asteroids_extension_format': 1, 'region': region,
                'assets': old['assets'] + entries}
    budget = sum(e['vertex_count'] * e.get('vertex_stride', 32) + e['index_count'] * 4
                 + e['width'] * e['height'] * 4 for e in manifest['assets'])
    if budget > 512 * 1024 * 1024: raise ValueError('Reader aggregate exceeds512MiB')
    extension = {'format': 1, 'region': region, 'base': str(base), 'levels': str(levels),
        'base_metadata_sha256': base_identity, 'base_inventory': old_inventory,
        'variants': records, 'variant_selection': 'Two stable native variants, never an interchangeable LOD pair',
        'normalization': 'One shared centre/divisor for every material of each variant; Z up; float32radius<=1',
        'atlas': {'rgba': 'atlas_asteroids.rgba.bin', 'width': atlas.width, 'height': atlas.height,
                  'gutter': GUTTER, 'tiles': {k: {'rect': rects[k], 'pixels': pixels[k]} for k in tiles},
                  'filter': 'GL_LINEAR, no mipmaps/aniso, matching0.3.3 transient atlas sampler'},
        'max_bilinear_error_byte_units': error, 'reader_aggregate_bytes': budget,
        'scope': 'Original Gemlik model212 plus model1412; all core/crater surfaces kept. No native gameplay, AI, damage, field, seed, motion or new shape.',
        'native_sources': inputs.check()}
    (out / 'assets.json').write_text(json.dumps(manifest, indent=2) + '\n')
    # Preserve the entire legacy TSV prefix byte-for-byte, then append rows.
    prefix = (base / 'assets.tsv').read_bytes()
    if not prefix.endswith(b'\n'): raise ValueError('Base TSV must end with newline')
    suffix = ''.join(' '.join(str(e[k]) for k in ('id','vertices','indices','rgba','width','height',
        'vertex_stride','wrap_s','wrap_t','alpha_mode')) + '\n' for e in entries).encode('ascii')
    (out / 'assets.tsv').write_bytes(prefix + suffix)
    (out / 'asteroid-extension.json').write_text(json.dumps(extension, indent=2) + '\n')
    (out / 'provenance.json').write_text(json.dumps({**provenance, 'asteroids': extension}, indent=2) + '\n')
    (out / 'inventory.json').write_text(json.dumps(inventory(out), indent=2) + '\n')
    validate_inventory(base, old_inventory)
    for name, sha in base_identity.items():
        if digest((base / name).read_bytes()) != sha: raise ValueError('Base metadata changed')
    return verify(out)

def verify(root):
    root = root.resolve(); manifest = json.loads((root / 'assets.json').read_text())
    extension = json.loads((root / 'asteroid-extension.json').read_text())
    validate_inventory(root, json.loads((root / 'inventory.json').read_text()))
    base = Path(extension['base']); old = json.loads((root / 'base-0.3.3.assets.json').read_text())
    for name, sha in extension['base_metadata_sha256'].items():
        if digest((root / ('base-0.3.3.' + name)).read_bytes()) != sha or digest((base / name).read_bytes()) != sha:
            raise ValueError('Base metadata was changed')
    validate_inventory(base, extension['base_inventory'])
    for name, expected in extension['base_inventory'].items():
        if name in METADATA: continue
        if digest((root / name).read_bytes()) != expected['sha256']:
            raise ValueError('An old payload was altered in the extended pack')
    if manifest['assets'][:-2] != old['assets']: raise ValueError('Base mesh entries changed')
    if not (root / 'assets.tsv').read_bytes().startswith((base / 'assets.tsv').read_bytes()):
        raise ValueError('Base TSV prefix changed')
    models, inputs = source_models(Path(extension['levels']), extension['region'])
    rects = {key: row['rect'] for key, row in extension['atlas']['tiles'].items()}
    atlas = Image.frombytes('RGBA', (extension['atlas']['width'], extension['atlas']['height']),
                            (root / 'atlas_asteroids.rgba.bin').read_bytes())
    counts = []
    for entry, model in zip(manifest['assets'][-2:], models):
        if entry['id'] != model['id'] or entry['local_bound_radius'] != 1:
            raise ValueError('Asteroid group/bound contract changed')
        vv, ii, _ = variant_buffers(model, rects)
        if (root / entry['vertices']).read_bytes() != vv or (root / entry['indices']).read_bytes() != ii:
            raise ValueError('Native geometry/UV/colour/index preservation mismatch')
        for row in struct.iter_unpack('<16f', vv):
            if not all(math.isfinite(v) for v in row) or math.sqrt(sum(v*v for v in row[:3])) > 1:
                raise ValueError('Invalid asteroid vertex/bound')
        for _, _, key, image, *_ in model['parts']:
            x, y, w, h = extension['atlas']['tiles'][key[0]]['pixels']
            if atlas.crop((x,y,x+w,y+h)).tobytes() != image.tobytes():
                raise ValueError('Original native texels changed')
            check_bilinear(image, atlas, rects[key[0]], REPEAT, REPEAT)
        counts.append({'id': entry['id'], 'class': model['class'], 'vertices': len(vv)//64,
                       'triangles': len(ii)//12, 'radius': entry['max_vertex_radius']})
    if inputs.check() != extension['native_sources']: raise ValueError('Native source drift')
    return {'output': str(root), 'region': extension['region'], 'verified': True,
            'base_payloads_unchanged': True, 'total_meshes': len(manifest['assets']),
            'reader_aggregate_bytes': extension['reader_aggregate_bytes'], 'variants': counts,
            'max_bilinear_error_byte_units': extension['max_bilinear_error_byte_units']}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', type=Path); p.add_argument('--levels', type=Path)
    p.add_argument('--region', choices=('pal','ntsc')); p.add_argument('--out', type=Path)
    p.add_argument('--verify', type=Path); args = p.parse_args()
    if args.verify: result = verify(args.verify)
    elif all((args.base,args.levels,args.region,args.out)):
        result = build(args.base,args.levels,args.region,args.out)
    else: p.error('Provide --base --levels --region --out, or --verify')
    print(json.dumps(result, indent=2))

if __name__ == '__main__': main()
