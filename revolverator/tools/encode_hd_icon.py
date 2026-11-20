#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Encode a locally generated 128px icon for the native indexed HUD renderer.

The header and binary are private, game-derived build inputs. Do not commit or
redistribute them with the public source. No image or upscaler is downloaded.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat


def encode(image: Image.Image) -> tuple[bytes, bytes, Image.Image]:
    image = image.convert('RGBA')
    if image.size != (128, 128):
        raise ValueError('The native HD icon must be exactly 128x128')
    # Reserve index zero for the transparent 32px placeholder. A single native
    # PSMT8 palette has 256 entries, so the visible glyph uses the other 255.
    rgb = image.convert('RGB')
    indexed = rgb.quantize(colors=255, method=Image.Quantize.MEDIANCUT,
                           dither=Image.Dither.NONE)
    palette = indexed.getpalette()
    pixels = bytes(value+1 for value in indexed.tobytes())
    clut = bytearray(1024)
    for index in range(255):
        native = index+1
        # CSM1 order is an involution: exchange palette-address bits 3 and 4.
        native = (native & ~24) | ((native & 8) << 1) | ((native & 16) >> 1)
        clut[native*4:native*4+4] = bytes(palette[index*3:index*3+3]+[128])
    # This selected native tile is fully opaque. Reject silent alpha loss if
    # a future selected glyph needs a different RGBA quantization strategy.
    if image.getextrema()[3] != (255, 255):
        raise ValueError('This encoder expects the selected opaque icon tile')
    decoded = Image.new('RGBA', image.size)
    values = []
    for index in pixels:
        slot = (index & ~24) | ((index & 8) << 1) | ((index & 16) >> 1)
        red, green, blue, alpha = clut[slot*4:slot*4+4]
        values.append((red, green, blue, min(255, alpha*2)))
    decoded.putdata(values)
    return pixels, bytes(clut), decoded


def array(name: str, data: bytes) -> str:
    rows = [','.join(str(n) for n in data[start:start+32])
            for start in range(0, len(data), 32)]
    return f'inline constexpr std::uint8_t {name}[]={{\n'+',\n'.join(rows)+'\n};\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    original = Image.open(args.image).convert('RGBA')
    pixels, palette, restored = encode(original)
    header = ('// Generated locally from game-derived artwork. Private build input.\n'
              '#pragma once\n#include "hd_icon.hpp"\n'
              'namespace revolverator::generated {\n'+
              array('icon_pixels', pixels)+array('icon_palette', palette)+
              'inline HdIconData hd_icon_data(){return {icon_pixels,sizeof icon_pixels,'
              'icon_palette,sizeof icon_palette};}\n}\n')
    (args.out/'revolverator_hd_icon_data.hpp').write_text(header, encoding='utf-8')
    (args.out/'revolverator-hd.psmt8').write_bytes(pixels)
    (args.out/'revolverator-hd.clut').write_bytes(palette)
    restored.save(args.out/'native-palette-preview.png')
    delta = ImageStat.Stat(ImageChops.difference(original.convert('RGB'),restored.convert('RGB')))
    report = {'texture_size':[128,128], 'logical_hud_size':[32,32],
        'colors':255, 'format':'PSMT8', 'clut':'CSM1 RGBA, alpha0..128',
        'transparent_index':0, 'mean_absolute_error_rgb':delta.mean,
        'pixels_sha256':hashlib.sha256(pixels).hexdigest(),
        'palette_sha256':hashlib.sha256(palette).hexdigest()}
    (args.out/'encoding.json').write_text(json.dumps(report,indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
