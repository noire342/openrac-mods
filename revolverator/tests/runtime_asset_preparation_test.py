#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Host tests for runtime asset derivation; does not need game data or Pillow."""
import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

script=Path(__file__).resolve().parents[1]/'tools/prepare_runtime_assets.py'
spec=importlib.util.spec_from_file_location('runtime_art',script)
runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime)

class TextureCodec(unittest.TestCase):
    def test_short_texture_uri_preserves_model_buffer(self):
        native_buffer=bytearray(range(16))
        model=runtime.write_glb({'images':[{'uri':'../textures/moby_revolverator_original.png'}]},native_buffer)
        shorter=runtime.model_texture(model,'../textures/base.png')
        import json
        size,kind=struct.unpack_from('<II',shorter,12)
        document=json.loads(shorter[20:20+size])
        self.assertEqual(document['images'][0]['uri'],'../textures/base.png')
        offset=20+size
        length,kind=struct.unpack_from('<II',shorter,offset)
        self.assertEqual(kind,0x004e4942)
        self.assertEqual(shorter[offset+8:offset+8+length],bytes(native_buffer))

    def test_rgba_round_trip_and_alpha(self):
        pixels=bytes([0,0,0,0,255,255,255,128,100,110,120,255,80,50,30,255])
        encoded=runtime.png_bytes(2,2,pixels)
        self.assertEqual(runtime.png_rgba(encoded),(2,2,pixels))
        width,height,gold=runtime.png_rgba(runtime.golden_texture(encoded))
        self.assertEqual((width,height),(2,2))
        self.assertEqual(gold[3::4],pixels[3::4])
        self.assertEqual(gold[:4],pixels[:4])
        for p in (4,8,12):self.assertGreater(gold[p],gold[p+1]);self.assertGreater(gold[p+1],gold[p+2])

    def test_all_native_png_scanline_filters(self):
        rows=[bytes([12,30,50,255,100,140,170,128]),bytes([13,70,90,100,200,220,245,0])]
        for kind in range(5):
            raw=bytearray();previous=bytes(8)
            for row in rows:
                raw.append(kind)
                for x,value in enumerate(row):
                    a=row[x-4] if x>=4 else 0;b=previous[x];c=previous[x-4] if x>=4 else 0
                    if kind==0:prediction=0
                    elif kind==1:prediction=a
                    elif kind==2:prediction=b
                    elif kind==3:prediction=(a+b)//2
                    else:
                        p=a+b-c;pa,pb,pc=abs(p-a),abs(p-b),abs(p-c)
                        prediction=a if pa<=pb and pa<=pc else b if pb<=pc else c
                    raw.append((value-prediction)&255)
                previous=row
            def chunk(tag,payload):return struct.pack('>I',len(payload))+tag+payload+struct.pack('>I',zlib.crc32(tag+payload)&0xffffffff)
            png=runtime.PNG_SIGNATURE+chunk(b'IHDR',struct.pack('>IIBBBBB',2,2,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
            self.assertEqual(runtime.png_rgba(png),(2,2,b''.join(rows)))

    def test_corruption_and_unbounded_images_fail(self):
        image=bytearray(runtime.png_bytes(1,1,bytes([0,100,200,255])))
        image[30]^=1
        with self.assertRaises(ValueError):runtime.png_rgba(bytes(image))
        with self.assertRaises(ValueError):runtime.png_rgba(b'not PNG')
        with self.assertRaises(ValueError):runtime.png_bytes(2,2,b'\0'*4)

if __name__=='__main__':unittest.main()
