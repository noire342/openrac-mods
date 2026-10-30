#!/usr/bin/env python3
"""Build original Revolverator ammunition art and an isolated native-level extension.

The installed levels and other mods are never modified. Standard model packs
cannot add class IDs to the current OpenRAC manifest, so this private QA overlay
adds world visual class 32000 and vendor alias 2016 to cloned manifests. Every existing large asset is linked
back to the installed data; no engine rebuild or game-asset extraction is needed.
Run OpenRAC with --levels OUT/levels and the normal Revolverator --mods pack.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess

from PIL import Image


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def normal(v):
    length = math.sqrt(sum(x*x for x in v))
    if length < 1e-10:
        raise ValueError('Degenerate triangle in generated ammunition model')
    return tuple(x/length for x in v)


class Mesh:
    def __init__(self):
        self.positions = []
        self.normals = []
        self.uvs = []
        self.indices = []

    def triangle(self, a, b, c, swatch):
        n = normal(cross(sub(b, a), sub(c, a)))
        x, y = (swatch % 4)*32, (swatch//4)*64
        # Each face spans a painted tile, avoiding colour bleeding at its edges.
        uvs = [((x+4)/128, (y+59)/128), ((x+27)/128, (y+59)/128), ((x+16)/128, (y+5)/128)]
        start = len(self.positions)
        self.positions.extend((a, b, c))
        self.normals.extend((n, n, n))
        self.uvs.extend(uvs)
        self.indices.extend((start, start+1, start+2))

    def quad(self, a, b, c, d, swatch):
        self.triangle(a, b, c, swatch)
        self.triangle(a, c, d, swatch)

    def cylinder(self, centre, radius, bottom, top, swatch, sides=12, upper_radius=None, caps=True):
        if upper_radius is None:
            upper_radius = radius
        cx, cy = centre
        for i in range(sides):
            a, b = 2*math.pi*i/sides, 2*math.pi*(i+1)/sides
            p = (cx+math.cos(a)*radius, cy+math.sin(a)*radius, bottom)
            q = (cx+math.cos(b)*radius, cy+math.sin(b)*radius, bottom)
            r = (cx+math.cos(b)*upper_radius, cy+math.sin(b)*upper_radius, top)
            s = (cx+math.cos(a)*upper_radius, cy+math.sin(a)*upper_radius, top)
            self.quad(p, q, r, s, swatch)
            if caps:
                self.triangle((cx, cy, bottom), q, p, swatch)
                self.triangle((cx, cy, top), s, r, swatch)

    def rivet(self, x, y, z, radius=.009):
        # Deliberately faceted, as R&C1's small mechanical details are.
        top, bottom = (x, y, z+radius), (x, y, z-radius)
        ring = [(x+radius, y, z), (x, y+radius, z), (x-radius, y, z), (x, y-radius, z)]
        for i in range(4):
            self.triangle(top, ring[i], ring[(i+1)%4], 2)
            self.triangle(bottom, ring[(i+1)%4], ring[i], 2)

    def radial_box(self, angle, width, bottom, top, radius, depth, swatch):
        """An enclosed rectangular cooling rib or inset on a cylinder face."""
        side = (-math.sin(angle), math.cos(angle))
        facing = (math.cos(angle), math.sin(angle))
        def point(r, lateral, z):
            return (facing[0]*r+side[0]*lateral,
                    facing[1]*r+side[1]*lateral, z)
        a, b = point(radius, -width/2, bottom), point(radius, width/2, bottom)
        c, d = point(radius, width/2, top), point(radius, -width/2, top)
        e, f = point(radius+depth, -width/2, bottom), point(radius+depth, width/2, bottom)
        g, h = point(radius+depth, width/2, top), point(radius+depth, -width/2, top)
        for face in [(e,f,g,h), (b,a,d,c), (a,b,f,e), (d,h,g,c), (a,e,h,d), (f,b,c,g)]:
            self.quad(*face, swatch)


def geometry(recipe):
    mesh = Mesh()
    radius, height = recipe['body_radius'], recipe['body_height']
    # One compact service cartridge: the motor consumes charge, not spare drill
    # tips. The endcaps and contact make its function readable at pickup scale.
    mesh.cylinder((0, 0), radius*1.10, .012, .037, 1)
    mesh.cylinder((0, 0), radius*1.075, .030, .060, 2)
    mesh.cylinder((0, 0), radius, .054, height-.018, 0)
    mesh.cylinder((0, 0), radius*1.075, height-.035, height, 2)
    mesh.cylinder((0, 0), radius*.86, height, height+.022, 2,
                  upper_radius=radius*.72)
    mesh.cylinder((0, 0), radius*.66, height+.019, height+.036, 6)
    mesh.cylinder((0, 0), radius*.47, height+.033, height+.073, 2)
    mesh.cylinder((0, 0), radius*.24, height+.073, height+.076, 6)
    # The front has a small amber charge gauge; the other six faces have heavy
    # cooling ribs, riveted into the same red/brass industrial casing as the gun.
    for angle in (0, math.pi/3, 2*math.pi/3, math.pi, 7*math.pi/6, 11*math.pi/6):
        mesh.radial_box(angle, .030, .063, height-.041, radius*.98, .012, 1)
        for z in (.084, height-.064):
            mesh.rivet((radius*1.075)*math.cos(angle), (radius*1.075)*math.sin(angle), z, .006)
    front = -math.pi/2
    mesh.radial_box(front, .074, .108, .252, radius*.97, .016, 1)
    mesh.radial_box(front, .054, .119, .241, radius*.97+.016, .002, 6)
    for z in (.131, .168, .205):
        mesh.radial_box(front, .040, z, z+.024, radius*.97+.018, .002, 4)
    return mesh


def texture(recipe):
    rng = random.Random(189164)
    picture = Image.new('RGBA', (128, 128))
    pixels = picture.load()
    for index, base in enumerate(recipe['palette']):
        x0, y0 = (index % 4)*32, (index//4)*64
        for y in range(64):
            for x in range(32):
                edge = min(x, 31-x)
                brightness = 1 + .17*(1-y/63) + (.11 if edge < 2 else 0)
                noise = rng.randrange(-8, 9)
                colour = tuple(max(0, min(255, int(c*brightness)+noise)) for c in base)
                if y in (9, 50) and 4<x<27:
                    colour = tuple(int(c*.70) for c in colour)
                pixels[x0+x, y0+y] = (*colour, 255)
        for _ in range(20):
            x, y = x0+rng.randrange(4, 28), y0+rng.randrange(4, 59)
            old = pixels[x, y]
            pixels[x, y] = (*[min(255, int(c*1.22)) for c in old[:3]], 255)
    return picture


def glb(mesh, scale):
    data = bytearray()
    views, accessors = [], []

    def add(values, fmt, kind, component, target):
        data.extend(b'\0'*(-len(data) % 4))
        start = len(data)
        for value in values:
            data.extend(struct.pack('<'+fmt, *value) if isinstance(value, tuple) else struct.pack('<'+fmt, value))
        view = len(views)
        views.append({'buffer': 0, 'byteOffset': start, 'byteLength': len(data)-start, 'target': target})
        entry = {'bufferView': view, 'componentType': component, 'count': len(values), 'type': kind}
        if kind == 'VEC3' and target == 34962:
            entry['min'] = [min(v[i] for v in values) for i in range(3)]
            entry['max'] = [max(v[i] for v in values) for i in range(3)]
        accessors.append(entry)
        return len(accessors)-1

    # Native dynamic mobys carry their Part scale in the instance matrix. Store
    # this mesh in the matching class space, exactly as extracted R&C1 GLBs do.
    positions = [tuple(v/scale for v in p) for p in mesh.positions]
    pos = add(positions, '3f', 'VEC3', 5126, 34962)
    norm = add(mesh.normals, '3f', 'VEC3', 5126, 34962)
    uv = add(mesh.uvs, '2f', 'VEC2', 5126, 34962)
    indices = add(mesh.indices, 'H', 'SCALAR', 5123, 34963)
    document = {
        'asset': {'version': '2.0', 'generator': 'Revolverator original ammunition builder'},
        'scene': 0, 'scenes': [{'nodes': [0]}],
        'nodes': [{'name': 'revolverator_energy_cartridge', 'mesh': 0}],
        'meshes': [{'name': 'revolverator_energy_cartridge', 'primitives': [{
            'attributes': {'POSITION': pos, 'NORMAL': norm, 'TEXCOORD_0': uv},
            'indices': indices, 'material': 0}]}],
        'materials': [{'name': 'revolverator_ammo', 'doubleSided': False,
            'pbrMetallicRoughness': {'baseColorTexture': {'index': 0},
                'metallicFactor': 0, 'roughnessFactor': 1}}],
        'images': [{'uri': 'texture.png'}], 'textures': [{'source': 0, 'sampler': 0}],
        'samplers': [{'magFilter': 9728, 'minFilter': 9728, 'wrapS': 33071, 'wrapT': 33071}],
        'buffers': [{'byteLength': len(data)}], 'bufferViews': views, 'accessors': accessors,
        'extras': {'game': 'rac1', 'visual_class': 32000, 'native_pickup_class': 225,
                   'authoring_metres': True, 'native_class_scale': scale}
    }
    encoded = json.dumps(document, separators=(',', ':')).encode()
    encoded += b' '*(-len(encoded) % 4)
    data.extend(b'\0'*(-len(data) % 4))
    return (struct.pack('<III', 0x46546c67, 2, 28+len(encoded)+len(data))+
            struct.pack('<II', len(encoded), 0x4e4f534a)+encoded+
            struct.pack('<II', len(data), 0x004e4942)+data)


def junction(source: Path, target: Path):
    if os.name == 'nt':
        quote = lambda p: "'"+str(p).replace("'", "''")+"'"
        command = f'New-Item -ItemType Junction -Path {quote(target)} -Target {quote(source)} | Out-Null'
        subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', command], check=True)
    else:
        target.symlink_to(source, target_is_directory=True)


def build(levels: Path, out: Path, recipe_path: Path):
    levels, out = levels.resolve(), out.resolve()
    if out.exists():
        raise ValueError('Output already exists. Choose a new output directory.')
    if out == levels or out.is_relative_to(levels):
        raise ValueError('Output must be separate from the installed game levels.')
    sources = sorted(p for p in levels.glob('level_*') if (p/'manifest.json').is_file())
    if not sources:
        raise ValueError('No extracted level manifests found.')
    before = {str(p/'manifest.json'): digest(p/'manifest.json') for p in sources}
    recipe = json.loads(recipe_path.read_text(encoding='utf8'))
    mesh, picture = geometry(recipe), texture(recipe)
    if len(mesh.positions) >= 65536:
        raise ValueError('Mesh exceeds its u16 index format.')
    out.mkdir(parents=True)
    shutil.copy2(recipe_path, out/'recipe.json')
    # A metre-space reference is convenient in DCC tools; the game variants below
    # are converted to their native Part scale.
    art = out/'art'
    art.mkdir()
    (art/'model.glb').write_bytes(glb(mesh, 1.0))
    picture.save(art/'texture.png')
    installed = out/'levels'
    installed.mkdir()
    added = []
    for source in sources:
        manifest = json.loads((source/'manifest.json').read_text(encoding='utf8'))
        if manifest.get('game') != 'rac1' or manifest.get('version') != 'ntsc' or manifest.get('format') != 3:
            raise ValueError(f'Unsupported manifest: {source}')
        dest = installed/source.name
        dest.mkdir()
        for child in source.iterdir():
            if child.name == 'manifest.json':
                continue
            target = dest/child.name
            if child.is_dir():
                junction(child, target)
            else:
                try:
                    os.link(child, target)
                except OSError:
                    shutil.copy2(child, target)
        info = manifest['classes']['moby'].get(str(recipe['native_pickup_class']))
        if info and int(manifest['level']) in range(19):
            identities = [recipe['visual_class']]
            if recipe.get('vendor_visual_class') is not None:
                identities.append(recipe['vendor_visual_class'])
            if len(set(identities)) != len(identities):
                raise ValueError('World and vendor visual class IDs must differ.')
            for identity in identities:
                if str(identity) in manifest['classes']['moby']:
                    raise ValueError(f'Visual class {identity} already used in {source.name}')
            scale = float(info['scale'])
            if not math.isfinite(scale) or scale <= 0:
                raise ValueError(f'Invalid native ammo scale: {source.name}')
            own = dest/'revolverator_ammo'
            own.mkdir()
            (own/'model.glb').write_bytes(glb(mesh, scale))
            picture.save(own/'texture.png')
            for identity in identities:
                manifest['classes']['moby'][str(identity)] = {
                    'mesh': 'revolverator_ammo/model.glb', 'scale': scale,
                    'name': 'revolverator_ammunition' if identity == recipe['visual_class']
                            else 'revolverator_ammunition_vendor_preview'}
            added.append({'level': manifest['level'], 'scale': scale, 'visual_classes': identities})
        (dest/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')
    after = {p: digest(Path(p)) for p in before}
    if before != after:
        raise RuntimeError('An installed level manifest changed during preparation.')
    receipt = {'format': 1, 'purpose': 'private isolated Revolverator ammunition QA overlay',
        'original_levels': str(levels), 'extended_levels': str(installed),
        'visual_class': recipe['visual_class'], 'native_pickup_class': recipe['native_pickup_class'],
        'vendor_visual_class': recipe.get('vendor_visual_class'),
        'triangles': len(mesh.indices)//3, 'vertices': len(mesh.positions),
        'bounds_metres': {'min': [min(p[i] for p in mesh.positions) for i in range(3)],
                          'max': [max(p[i] for p in mesh.positions) for i in range(3)]},
        'levels_added': added, 'original_manifests_sha256': before,
        'original_manifests_unchanged': True,
        'standalone_launcher_model_pack': False}
    (out/'build-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf8')
    (out/'README.txt').write_text(
        'Revolverator original ammunition art and isolated asset extension.\n'
        'Launch OpenRAC with --levels "'+str(installed)+'" and the usual Revolverator --mods pack.\n'
        'The overlay links existing assets read-only by convention and owns only cloned manifests and new art.\n'
        'Do not open this QA overlay as an editable level project. The installed game was not modified.\n'
        'Native runtime creates a normal class225 ammo pickup, then uses visual class32000 for slot18 only.\n'
        'Vendor preview visual class2016 uses the same energy-cartridge GLB and native225scale.\n'
        'This is a private QA extension, not a standard launcher-importable new-class pack.\n', encoding='utf8')
    print(json.dumps({k: receipt[k] for k in ('extended_levels','visual_class','vendor_visual_class','triangles','vertices','bounds_metres','original_manifests_unchanged')}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--levels', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--recipe', type=Path, default=Path(__file__).resolve().parents[1]/'assets/ammunition/recipe.json')
    args = parser.parse_args()
    build(args.levels, args.out, args.recipe)


if __name__ == '__main__':
    main()
