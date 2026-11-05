#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 lorenzogarrone
"""Derive Rotatore art locally from the player's extracted disc, using stdlib only.

No game artwork is bundled with this script. The official Windows install's
own Python runs it before the native manifest overlay is published. Inputs are
read-only, the content-addressed cache is outside game and mod directories, and
an atomic receipt is published only after every generated file is complete.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_model_pack import read_glb, retarget, write_glb
from build_golden_assets import golden_model

RECIPE_VERSION = 1
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def beneath(path: Path, root: Path) -> Path:
    resolved = path.resolve(strict=True)
    if not resolved.is_relative_to(root) or not resolved.is_file():
        raise ValueError('Native asset escapes the extracted levels directory')
    return resolved

def png_rgba(data: bytes) -> tuple[int,int,bytes]:
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError('Native texture is not PNG')
    offset, compressed, palette, alpha = 8, bytearray(), b'', b''
    width = height = depth = color = 0
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError('Truncated PNG chunk')
        length, = struct.unpack_from('>I',data,offset)
        tag, payload = data[offset+4:offset+8],data[offset+8:offset+8+length]
        if offset+12+length > len(data) or zlib.crc32(tag+payload)&0xffffffff != struct.unpack_from('>I',data,offset+8+length)[0]:
            raise ValueError('Invalid PNG chunk checksum')
        if tag == b'IHDR':
            width,height,depth,color,comp,filt,interlace = struct.unpack('>IIBBBBB',payload)
            if not (0 < width <= 4096 and 0 < height <= 4096) or depth != 8 or color not in (0,2,3,4,6) or comp or filt or interlace:
                raise ValueError('Unsupported native PNG layout')
        elif tag == b'IDAT': compressed.extend(payload)
        elif tag == b'PLTE': palette = payload
        elif tag == b'tRNS': alpha = payload
        offset += length+12
        if tag == b'IEND':break
    channels = {0:1,2:3,3:1,4:2,6:4}.get(color,0)
    stride = width*channels
    expected = height*(stride+1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(bytes(compressed),expected+1)
    if len(raw)!=expected or not decoder.eof:
        raise ValueError('Invalid or oversized native PNG image data')
    output = bytearray(width*height*4)
    previous = bytearray(stride)
    for y in range(height):
        at=y*(stride+1);kind=raw[at];row=bytearray(raw[at+1:at+1+stride])
        for x in range(stride):
            left=row[x-channels] if x>=channels else 0;up=previous[x];upper_left=previous[x-channels] if x>=channels else 0
            if kind==1: predict=left
            elif kind==2:predict=up
            elif kind==3:predict=(left+up)//2
            elif kind==4:
                p=left+up-upper_left;a,b,c=abs(p-left),abs(p-up),abs(p-upper_left)
                predict=left if a<=b and a<=c else up if b<=c else upper_left
            elif kind==0:predict=0
            else:raise ValueError('Invalid PNG filter')
            row[x]=(row[x]+predict)&255
        for x in range(width):
            start=x*channels;value=row[start:start+channels];target=(y*width+x)*4
            if color==6: rgba=value
            elif color==2:rgba=value+b'\xff'
            elif color==0:rgba=bytes([value[0]])*3+b'\xff'
            elif color==4:rgba=bytes([value[0]])*3+bytes([value[1]])
            else:
                index=value[0]
                if index*3+3>len(palette):raise ValueError('Invalid native PNG palette')
                rgba=palette[index*3:index*3+3]+bytes([alpha[index] if index<len(alpha) else 255])
            output[target:target+4]=rgba
        previous=row
    return width,height,bytes(output)

def png_bytes(width: int,height: int,pixels: bytes) -> bytes:
    if len(pixels)!=width*height*4:raise ValueError('RGBA byte count differs')
    def chunk(tag,payload):
        return struct.pack('>I',len(payload))+tag+payload+struct.pack('>I',zlib.crc32(tag+payload)&0xffffffff)
    filtered=b''.join(b'\0'+pixels[y*width*4:(y+1)*width*4] for y in range(height))
    return PNG_SIGNATURE+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(filtered,9))+chunk(b'IEND',b'')

def golden_texture(native: bytes) -> bytes:
    width,height,rgba=png_rgba(native)
    output=bytearray(rgba)
    # Authored metal colour ramp; only the player's texture contributes its
    # shading, UV detail and alpha. No copied/recoloured atlas ships in the ZIP.
    for at in range(0,len(rgba),4):
        r,g,b,a=rgba[at:at+4]
        if not a:continue
        value=(r*.2126+g*.7152+b*.0722)/255
        output[at]=round(255*min(1,value*1.22+.035))
        output[at+1]=round(255*min(1,value*.91+.012))
        output[at+2]=round(255*min(1,value*.32+.004))
    return png_bytes(width,height,bytes(output))

def model_texture(blob: bytes, uri: str) -> bytes:
    """Change only the local texture URI, retaining the native mesh buffer."""
    size,kind=struct.unpack_from('<II',blob,12)
    if kind!=0x4e4f534a:raise ValueError('Generated GLB has no JSON chunk')
    document=json.loads(blob[20:20+size]);offset=20+size
    length,kind=struct.unpack_from('<II',blob,offset)
    if kind!=0x004e4942:raise ValueError('Generated GLB has no binary chunk')
    document['images'][0]['uri']=uri
    return write_glb(document,bytearray(blob[offset+8:offset+8+length]))

def prepare(levels: Path, cache: Path, version: str) -> Path:
    levels=levels.resolve(strict=True);cache=cache.resolve()
    module=Path(__file__).resolve().parents[1]
    if cache.is_relative_to(levels) or cache.is_relative_to(module):
        raise ValueError('Derived cache must be outside installed game and mod files')
    source_root=levels/'level_12'
    manifests=[]
    inputs={}
    for i in range(19):
        path=beneath(levels/f'level_{i:02}'/'manifest.json',levels)
        blob=path.read_bytes();m=json.loads(blob)
        if m.get('format') not in (3,4) or m.get('game')!='rac1' or m.get('version')!=version or m.get('level')!=i:
            raise ValueError('Expected consistent extracted native level manifests')
        inputs[str(path)]=digest(blob);manifests.append(m)
    source_info=manifests[12]['classes']['moby']['164']
    source_path=beneath(source_root/source_info['mesh'],levels)
    source=read_glb(source_path)
    texture_path=beneath(source_path.parent/source[0]['images'][0]['uri'],levels)
    native_texture=texture_path.read_bytes()
    inputs[str(source_path)]=digest(source_path.read_bytes());inputs[str(texture_path)]=digest(native_texture)
    targets=[]
    for i,m in enumerate(manifests):
        info=m['classes']['moby']['180'];path=beneath(levels/f'level_{i:02}'/info['mesh'],levels)
        inputs[str(path)]=digest(path.read_bytes());targets.append((info,read_glb(path)))
    for filename in ('prepare_runtime_assets.py','build_model_pack.py','build_golden_assets.py'):
        path=Path(__file__).parent/filename;inputs[filename]=digest(path.read_bytes())
    key=digest(json.dumps({'recipe':RECIPE_VERSION,'version':version,'inputs':inputs},sort_keys=True).encode())
    # Native Windows file readers may still use MAX_PATH even though Python's
    # filesystem APIs accept longer names. Keep only a 96-bit directory label;
    # the complete input SHA-256 remains mandatory inside READY.json. This also
    # leaves room for level/texture names under long launcher profile paths.
    root=cache/key[:24]
    ready=root/'READY.json'
    expected_files={f'level_{i:02}/{part}' for i in range(19) for part in
                    ('mobys/moby_2017.glb','mobys/moby_2018.glb','textures/base.png','textures/gold.png')}
    expected_files.add('level_01/mobys/moby_2019.glb')
    if ready.is_file():
        try:receipt=json.loads(ready.read_text())
        except (ValueError,OSError):receipt={}
        if receipt.get('key')==key and set(receipt.get('files',{}))==expected_files:
            if all((root/p).is_file() and digest((root/p).read_bytes())==sha for p,sha in receipt['files'].items()):
                return root
    cache.mkdir(parents=True,exist_ok=True)
    # A new session never consumes an incomplete directory. A failed prior
    # derivation is left for diagnostics, rather than deleting computed paths.
    root.mkdir(exist_ok=True)
    result={}
    golden=golden_texture(native_texture)
    for i,(info,target) in enumerate(targets):
        base=root/f'level_{i:02}'
        for part in ('mobys','textures'):(base/part).mkdir(exist_ok=True,parents=True)
        model=retarget(source,target,float(source_info['scale'])/float(info['scale']))
        normal=base/'mobys/moby_2017.glb';normal.write_bytes(model)
        gold,_=golden_model(normal);(base/'mobys/moby_2018.glb').write_bytes(model_texture(gold,'../textures/gold.png'))
        (base/'textures/base.png').write_bytes(native_texture)
        (base/'textures/gold.png').write_bytes(golden)
        if i==1:
            display,_=golden_model(normal,True);(base/'mobys/moby_2019.glb').write_bytes(model_texture(display,'../textures/gold.png'))
        normal.write_bytes(model_texture(model,'../textures/base.png'))
    for relative in sorted(expected_files):result[relative]=digest((root/relative).read_bytes())
    if len(result)!=77:raise ValueError('Incomplete generated model set')
    receipt={'format':1,'key':key,'version':version,'files':result,'inputs':inputs,'derived_on_player_machine':True}
    fd,temp=tempfile.mkstemp(prefix='ready-',suffix='.json',dir=root)
    with os.fdopen(fd,'w',encoding='utf-8') as f:json.dump(receipt,f,indent=2);f.write('\n')
    os.replace(temp,ready)
    # Every native input is still exactly the file used to create the cache.
    for name,expected in inputs.items():
        if name.endswith('.py') and not Path(name).is_absolute():continue
        if digest(Path(name).read_bytes())!=expected:raise ValueError('Native source changed while preparing art')
    return root

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--levels',type=Path,required=True);p.add_argument('--cache',type=Path,required=True)
    p.add_argument('--version',choices=('pal','ntsc'),required=True);p.add_argument('--result',type=Path,required=True)
    a=p.parse_args();root=prepare(a.levels,a.cache,a.version)
    a.result.parent.mkdir(exist_ok=True,parents=True)
    a.result.write_text(str(root),encoding='utf-8')
    print(json.dumps({'derived_on_player_machine':True,'version':a.version,'generated_files':77,'key':root.name}))

if __name__=='__main__':main()
