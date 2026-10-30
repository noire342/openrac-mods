#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 noire342
"""Extract the selected native glyph and upscale it with Real-ESRGAN ncnn Vulkan.

The source is the exact 59901:13 frame in a private native RAM fixture, not a crop
of the inspection contact sheet. Generated images remain private game-derived
assets. The original RAM capture is never modified.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from PIL import Image, ImageDraw


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(ram: bytes, state: int) -> tuple[Image.Image, dict]:
    def u32(at):
        if at < 0x10000 or at+4 > len(ram):
            raise ValueError(f'Invalid native address {at:#x}')
        return struct.unpack_from('<I',ram,at)[0]
    defs, refs, images, palettes = (u32(state+offset) for offset in (0x1c,0x20,0x24,0x28))
    selected = None
    for i in range(1024):
        identity,count,start=struct.unpack_from('<HHH',ram,defs+i*8)
        if identity==65535:
            break
        if identity==59901:
            if count<=13:
                raise ValueError('Native icon bank has no frame13')
            selected=start+13
            break
    if selected is None:
        raise ValueError('Native icon bank59901 was not found')
    palette,image=struct.unpack_from('<hh',ram,refs+selected*4)
    pixels,gs,lw,lh=struct.unpack_from('<IHBB',ram,images+image*8)
    clut=u32(palettes+palette*8)
    if pixels & 0x80000000 or clut & 0x80000000:
        raise ValueError('The source icon bank has not been loaded')
    width,height=1<<lw,1<<lh
    if (width,height)!=(32,32) or pixels+width*height>len(ram) or clut+1024>len(ram):
        raise ValueError('Expected one loaded native32x32 PSMT8 image')
    if gs:
        raise ValueError('This image has been cached/repacked; use an untouched original RAM fixture')
    rgba=[]
    for index in ram[pixels:pixels+width*height]:
        # The game's 256-entry CLUT uses CSM1 order, with bits3/4 exchanged.
        index=(index & ~24)|((index & 8)<<1)|((index & 16)>>1)
        r,g,b,a=ram[clut+index*4:clut+index*4+4]
        rgba.append((r,g,b,min(255,a*2)))
    icon=Image.new('RGBA',(width,height));icon.putdata(rgba)
    return icon,{'bank':59901,'frame':13,'width':width,'height':height,
        'native_frame':selected,'image_page':image,'palette_page':palette,
        'pixels':pixels,'clut':clut}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ram',type=Path,required=True)
    ap.add_argument('--hud-state',type=lambda value:int(value,0),default=0x17e958)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--esrgan',type=Path,required=True)
    ap.add_argument('--models',type=Path,required=True)
    ap.add_argument('--model',default='realesrgan-x4plus-anime')
    args=ap.parse_args()
    if args.out.exists():
        raise ValueError('Choose a new output directory')
    icon,receipt=extract(args.ram.read_bytes(),args.hud_state)
    args.out.mkdir(parents=True)
    source=args.out/'revolverator-icon-native32.png'
    result=args.out/'revolverator-icon-esrgan128.png'
    icon.save(source)
    command=[str(args.esrgan.resolve()),'-i',str(source.resolve()),'-o',str(result.resolve()),
        '-m',str(args.models.resolve()),'-n',args.model,'-s','4','-t','32','-j','1:1:1']
    run=subprocess.run(command,capture_output=True,text=True,check=True)
    restored=Image.open(result).convert('RGBA')
    if restored.size!=(128,128):
        raise ValueError('Real-ESRGAN did not produce a128x128 icon')
    # NCNN handles RGB here; preserve native transparency with the same upscale
    # footprint rather than allowing opaque data outside the original glyph.
    if icon.getextrema()[3]!=(255,255):
        restored.putalpha(icon.getchannel('A').resize(restored.size,Image.Resampling.BICUBIC))
        restored.save(result)
    compare=Image.new('RGB',(660,380),'#30343f');draw=ImageDraw.Draw(compare)
    for x,picture,label in [(20,icon.resize((288,288),Image.Resampling.NEAREST),'Native32x32 (nearest preview)'),
                             (350,restored.resize((288,288),Image.Resampling.NEAREST),'Real-ESRGAN4x,128x128')]:
        compare.paste(picture,(x,36),picture);draw.text((x,12),label,fill='white')
    draw.text((20,340),'Exact native glyph59901:13; logical HUD footprint stays32x32.',fill='white')
    compare.save(args.out/'comparison.png')
    receipt.update({'source_sha256':sha(source),'output_sha256':sha(result),
        'runtime_sha256':sha(args.esrgan),'model':args.model,
        'model_param_sha256':sha(args.models/(args.model+'.param')),
        'model_weights_sha256':sha(args.models/(args.model+'.bin')),
        'scale':4,'output_size':[128,128],
        'upstream':'https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan',
        'command':command,'process_exit_code':run.returncode})
    (args.out/'provenance.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (args.out/'process.log').write_text(run.stdout+run.stderr)
    print(json.dumps({'source':str(source),'result':str(result),'sha256':sha(result),'model':args.model},indent=2))


if __name__=='__main__':
    main()
