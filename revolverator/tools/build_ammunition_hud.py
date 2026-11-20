#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Render the existing cartridge GLB into a native transparent32px HUD icon.

No artwork is generated or downloaded. Blender renders the same locally built
model/texture used by the native ammunition pickup. The cartridge is original
authored mod artwork; its generated palette/header may ship with the mod source.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def render(model: Path, out: Path) -> None:
    import bpy
    from mathutils import Matrix, Quaternion, Vector

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(model.resolve()))
    # Native OpenRAC GLBs retain Z-up coordinates; cancel the glTF importer's
    # Y-up conversion, exactly as the existing native ammunition preview does.
    for root in [obj for obj in bpy.context.scene.objects if obj.parent is None]:
        root.matrix_world = Matrix.Rotation(-math.pi / 2, 4, 'X') @ root.matrix_world
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    if not meshes:
        raise ValueError('The supplied native GLB has no mesh')
    points = [obj.matrix_world @ vertex.co for obj in meshes for vertex in obj.data.vertices]
    low = Vector([min(point[i] for point in points) for i in range(3)])
    high = Vector([max(point[i] for point in points) for i in range(3)])
    centre = (low + high) * .5
    extent = max(high - low)
    if not math.isfinite(extent) or extent <= 0:
        raise ValueError('Invalid native cartridge bounds')
    bpy.ops.object.camera_add(location=centre + Vector((1.8, -2.8, 1.9)) * extent)
    camera = bpy.context.object
    camera.rotation_mode = 'QUATERNION'
    camera.rotation_quaternion = ((centre - camera.location).to_track_quat('-Z', 'Y')
                                  @ Quaternion((0, 0, 1), math.radians(-10)))
    camera.data.type = 'ORTHO'
    # Frame the rotated projected silhouette instead of guessing a world-axis
    # scale. This preserves two pixels of clearance at native resolution.
    rotation = camera.rotation_quaternion.to_matrix().transposed()
    projected = [rotation @ (point - centre) for point in points]
    span = [max(point[i] for point in projected) - min(point[i] for point in projected)
            for i in (0, 1)]
    midpoint = [(max(point[i] for point in projected) + min(point[i] for point in projected)) * .5
                for i in (0, 1)]
    camera.location += camera.rotation_quaternion @ Vector((midpoint[0], midpoint[1], 0))
    camera.data.ortho_scale = max(span) * 1.23
    scene = bpy.context.scene
    scene.camera = camera
    for direction, energy, size in [((-2.1, -3.0, 3.8), 420, 2.6),
                                    ((2.4, -.8, 1.8), 165, 2.2),
                                    ((.8, 2.4, 2.8), 280, 1.8)]:
        bpy.ops.object.light_add(type='AREA', location=centre + Vector(direction) * extent)
        light = bpy.context.object
        light.data.energy = energy * extent * extent
        light.data.size = extent * size
        light.rotation_euler = (centre - light.location).to_track_quat('-Z', 'Y').to_euler()
    scene.world = bpy.data.worlds.new('Native cartridge lighting')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.32, .35, .4, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .55
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.image_settings.color_depth = '8'
    scene.view_settings.view_transform = 'Standard'
    scene.render.filepath = str((out / 'ammunition-render512.png').resolve())
    bpy.ops.render.render(write_still=True)
    receipt = {'renderer': bpy.app.version_string, 'mesh_count': len(meshes),
               'vertices': len(points), 'bounds_min': list(low), 'bounds_max': list(high),
               'camera': list(camera.location), 'orthographic_scale': camera.data.ortho_scale,
               'camera_roll_degrees': -10, 'render_size': [512, 512],
               'transparent_background': True, 'model_modified': False}
    (out / 'render.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')


def quantize_rgba(image):
    """Deterministic weighted median cut plus Lloyd refinement in associated RGBA.

    An alpha-aware distance avoids spending colors on invisible matte RGB. Only
    the final small32px image is quantized; this needs Pillow and the standard
    library, without an external quantizer or runtime dependency.
    """
    from collections import Counter
    colors = Counter(getattr(image, 'get_flattened_data', image.getdata)())
    colors.pop((0, 0, 0, 0), None)
    colors = {color: count for color, count in colors.items() if color[3]}
    if not colors:
        raise ValueError('The native model rendered no visible ammunition')
    entries = list(colors)
    weights = [colors[color] for color in entries]
    points = [(color[0] * color[3] / 255, color[1] * color[3] / 255,
               color[2] * color[3] / 255, float(color[3])) for color in entries]
    def axis_score(group):
        mass = sum(weights[i] for i in group)
        mean = [sum(points[i][axis] * weights[i] for i in group) / mass for axis in range(4)]
        variance = [sum((points[i][axis] - mean[axis]) ** 2 * weights[i] for i in group)
                    * (1 if axis < 3 else .5) for axis in range(4)]
        axis = max(range(4), key=variance.__getitem__)
        return sum(variance), axis
    groups = [list(range(len(entries)))]
    while len(groups) < min(255, len(entries)):
        scores = [(axis_score(group) if len(group) > 1 else (-1, 0)) for group in groups]
        at = max(range(len(groups)), key=lambda i: scores[i][0])
        group = sorted(groups[at], key=lambda i: points[i][scores[at][1]])
        half = sum(weights[i] for i in group) / 2
        mass = 0
        split = 1
        for split in range(1, len(group)):
            mass += weights[group[split - 1]]
            if mass >= half:
                break
        groups[at] = group[:split]
        groups.append(group[split:])
    def centre(group):
        mass = sum(weights[i] for i in group)
        value = [sum(points[i][axis] * weights[i] for i in group) / mass for axis in range(4)]
        alpha = min(128, max(1, round(value[3] * 128 / 255)))
        return tuple(min(255, max(0, round(channel * 255 / value[3]))) for channel in value[:3]) + (alpha,)
    palette = [centre(group) for group in groups]
    assignments = []
    for iteration in range(4):
        vectors = [(red * min(255, alpha * 2) / 255, green * min(255, alpha * 2) / 255,
                    blue * min(255, alpha * 2) / 255, float(min(255, alpha * 2)))
                   for red, green, blue, alpha in palette]
        updated = []
        for red, green, blue, alpha in points:
            best, distance = 0, float('inf')
            for at, (pr, pg, pb, pa) in enumerate(vectors):
                metric = (red - pr) ** 2 + (green - pg) ** 2 + (blue - pb) ** 2 + .5 * (alpha - pa) ** 2
                if metric < distance:
                    best, distance = at, metric
            updated.append(best)
        if updated == assignments:
            break
        assignments = updated
        if iteration != 3:
            groups = [[] for _ in palette]
            for i, at in enumerate(assignments):
                groups[at].append(i)
            palette = [centre(group) if group else palette[i] for i, group in enumerate(groups)]
    indices = {color: assignments[i] + 1 for i, color in enumerate(entries)}
    return bytes(indices.get(color, 0) for color in getattr(image, 'get_flattened_data', image.getdata)()), palette


def encode(out: Path) -> dict:
    from PIL import Image

    rendered = Image.open(out / 'ammunition-render512.png').convert('RGBA')
    # Explicit associated-alpha resizing prevents a dark matte at the contour.
    preview = rendered.convert('RGBa').resize((128, 128), Image.Resampling.LANCZOS).convert('RGBA')
    original = rendered.convert('RGBa').resize((32, 32), Image.Resampling.LANCZOS).convert('RGBA')
    # Sub-one-step native-alpha Lanczos ringing is invisible geometry, not
    # part of the silhouette. Remove it to retain a clean transparent margin.
    original.putalpha(original.getchannel('A').point(lambda value: 0 if value <= 2 else value))
    preview.save(out / 'ammunition-preview128.png')
    original.save(out / 'ammunition-source32.png')
    pixels, rgba_palette = quantize_rgba(original)
    clut = bytearray(1024)
    linear = [(0, 0, 0, 0)] * 256
    for index, (red, green, blue, native_alpha) in enumerate(rgba_palette):
        logical = index + 1
        # PS2 CSM1 swaps palette address bits 3 and 4.
        native = (logical & ~24) | ((logical & 8) << 1) | ((logical & 16) >> 1)
        clut[native * 4:native * 4 + 4] = bytes((red, green, blue, native_alpha))
        linear[logical] = (red, green, blue, min(255, native_alpha * 2))
    decoded = Image.new('RGBA', (32, 32))
    decoded.putdata([linear[index] for index in pixels])
    if decoded.getbbox() is None or decoded.getextrema()[3][0] != 0:
        raise ValueError('Native icon must have a visible cartridge and transparent background')
    indexed = Image.frombytes('P', (32, 32), pixels)
    indexed.putpalette([component for color in linear for component in color[:3]])
    indexed.info['transparency'] = bytes(color[3] for color in linear)
    indexed.save(out / 'ammunition-icon32.png', optimize=True)
    decoded.save(out / 'native-palette-preview32.png')
    if Image.open(out / 'ammunition-icon32.png').convert('RGBA').tobytes() != decoded.tobytes():
        raise ValueError('Indexed PNG and native CSM1 encoding disagree')
    (out / 'ammunition.psmt8').write_bytes(pixels)
    (out / 'ammunition.clut').write_bytes(clut)
    native = out / 'native-data'
    native.mkdir(exist_ok=True)
    def array(name: str, data: bytes) -> str:
        rows = [','.join(str(value) for value in data[start:start + 32])
                for start in range(0, len(data), 32)]
        return 'inline constexpr std::uint8_t ' + name + '[]={\n' + ',\n'.join(rows) + '\n};\n'
    header = ('// SPDX-License-Identifier: GPL-3.0-or-later\n'
              '// Copyright (c) 2026 noire342\n'
              '// Generated from the original authored Revolverator ammunition model.\n'
              '// Recipe: tools/build_ammunition_hud.py; no original game artwork.\n'
              '#pragma once\n#include "hd_icon.hpp"\nnamespace revolverator::generated {\n'
              + array('ammunition_pixels', pixels) + array('ammunition_palette', bytes(clut))
              + 'inline HdIconData ammunition_icon_data(){return {ammunition_pixels,sizeof ammunition_pixels,'
                'ammunition_palette,sizeof ammunition_palette};}\n}\n')
    (native / 'revolverator_ammo_icon_data.hpp').write_text(header, encoding='utf-8')
    actual = list(getattr(decoded, 'get_flattened_data', decoded.getdata)())
    source = list(getattr(original, 'get_flattened_data', original.getdata)())
    # Associated RGBA error measures the image that is actually composited,
    # without counting arbitrary RGB values in fully transparent pixels.
    absolute = [0., 0., 0., 0.]
    for before, after in zip(source, actual):
        for channel in range(3):
            absolute[channel] += abs(before[channel] * before[3] / 255 - after[channel] * after[3] / 255)
        absolute[3] += abs(before[3] - after[3])
    return {'native_size': [32, 32], 'preview_size': [128, 128],
            'format': 'PSMT8', 'clut': 'CSM1 RGBA, alpha0..128',
            'transparent_index': 0, 'used_palette_indices': len(set(pixels)),
            'discarded_resampling_alpha_max': 2,
            'nontransparent_bounds': list(decoded.getbbox()),
            'mean_absolute_error_associated_rgba': [value / 1024 for value in absolute],
            'pixels_sha256': hashlib.sha256(pixels).hexdigest(),
            'palette_sha256': hashlib.sha256(clut).hexdigest()}


def main() -> None:
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--blender', type=Path)
    parser.add_argument('--render-stage', action='store_true')
    parser.add_argument('--overwrite', action='store_true')
    options = parser.parse_args(args)
    if options.render_stage:
        render(options.model, options.out)
        return
    if not options.blender or not options.blender.is_file() or not options.model.is_file():
        raise ValueError('Existing Blender executable and native GLB are required')
    if options.out.exists() and not options.overwrite:
        raise ValueError('Choose a fresh output directory or explicitly pass --overwrite')
    options.out.mkdir(parents=True, exist_ok=True)
    command = [str(options.blender.resolve()), '--background', '--factory-startup',
               '--python', str(Path(__file__).resolve()), '--', '--render-stage',
               '--model', str(options.model.resolve()), '--out', str(options.out.resolve())]
    run = subprocess.run(command, capture_output=True, text=True)
    (options.out / 'blender.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    run.check_returncode()
    report = encode(options.out)
    texture = options.model.with_name('texture.png')
    report.update({'model': str(options.model.resolve()), 'model_sha256': digest(options.model),
                   'texture_sha256': digest(texture) if texture.is_file() else None,
                   'blender_sha256': digest(options.blender), 'command': command,
                   'model_modified': False, 'new_artwork_generated': False})
    (options.out / 'encoding.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
