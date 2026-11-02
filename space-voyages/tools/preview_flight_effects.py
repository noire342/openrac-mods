#!/usr/bin/env python3
"""Offline native ship/nozzle diagnostic; these are NOT gameplay screenshots.

Requires optional NumPy, like preview_asteroids. Cyan rings show the extracted
aperture and orange arrows its exact outward normal. No flame is fabricated.
"""
import argparse,json,math,struct
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from preview_asteroids import render

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--assets',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    if a.out.exists():raise ValueError('Refusing to overwrite a diagnostic')
    a.out.mkdir(parents=True)
    manifest=json.loads((a.assets/'assets.json').read_text());ext=json.loads((a.assets/'flightfx-extension.json').read_text())
    sheet=Image.new('RGB',(900,1075),(20,24,32));draw=ImageDraw.Draw(sheet)
    draw.text((8,6),'OFFLINE original mesh/texture diagnostic. Cyan = aperture; orange = outward direction. Not gameplay.',fill='white')
    for row,cl in enumerate((531,532,533)):
        entries=[e for e in manifest['assets'] if e['id']==f'ship_{cl}' or e['id'].startswith(f'ship_{cl}_part')]
        if len(entries)!=1:raise ValueError('This diagnostic expects the original single-material native ship')
        e=entries[0];v=np.array(list(struct.iter_unpack('<8f',(a.assets/e['vertices']).read_bytes())))
        v=np.column_stack((v,np.ones((len(v),4)),np.tile([1,1,0,0],(len(v),1))));v[:,:3]*=.7
        ii=np.frombuffer((a.assets/e['indices']).read_bytes(),dtype='<u4')
        tex=np.asarray(Image.frombytes('RGBA',(e['width'],e['height']),(a.assets/e['rgba']).read_bytes()))
        for col,azimuth in enumerate((180,270,145)):
            image=render(v,ii,tex,azimuth,300);d=ImageDraw.Draw(image)
            az=math.radians(azimuth);el=math.radians(23)
            eye=np.array((math.cos(az)*math.cos(el),math.sin(az)*math.cos(el),math.sin(el)))
            right=np.array((-math.sin(az),math.cos(az),0));up=np.cross(eye,right)
            def project(point):
                point=np.asarray(point)*.7
                return float(point@right*300*.43+150),float(300-(point@up*300*.43+150))
            for j,m in enumerate(ext['ships'][str(cl)]['mounts']):
                position=np.array(m['position']);direction=np.array(m['direction'])
                basis=np.cross(direction,np.array((0,0,1)))
                if np.linalg.norm(basis)<.01:basis=np.cross(direction,np.array((0,1,0)))
                basis/=np.linalg.norm(basis);second=np.cross(direction,basis)
                ring=[project(position+m['radius']*(basis*math.cos(t*math.tau/32)+second*math.sin(t*math.tau/32))) for t in range(33)]
                d.line(ring,fill=(0,255,255),width=1);d.line((project(position),project(position+direction*m['length'])),fill=(255,170,30),width=2)
                x,y=project(position);d.text((x+2,y+2),str(j),fill='cyan')
            sheet.paste(image,(col*300,row*350+50));draw.text((col*300+8,row*350+32),f'Native ship {cl} / az {azimuth} / {len(ext["ships"][str(cl)]["mounts"])} ports',fill='white')
    sheet.save(a.out/'native-nozzle-mounts.png')
    ii=(a.assets/'health.indices.bin').read_bytes();pp=(a.assets/'health.palette.rgba.bin').read_bytes()
    radar=Image.frombytes('RGBA',(128,128),(a.assets/'radar.rgba.bin').read_bytes())
    # Original DrawGemlikTurretHud uses 0x70808080 on BOTH quads:
    # neutral RGB128 and vertex alpha112/128. Opaque FX50 would hide FX49.
    radar.putalpha(radar.getchannel('A').point(lambda x:round(x*112/128)))
    health=Image.new('RGB',(768,288),(20,24,32));hd=ImageDraw.Draw(health)
    for col,fill in enumerate((253,127,2)):
        rgba=b''.join(pp[i*4:i*4+4] if i<fill or i<2 else bytes((0,0,0,255)) for i in ii)
        image=Image.frombytes('RGBA',(128,128),rgba)
        image.putalpha(image.getchannel('A').point(lambda x:round(x*112/128)))
        background=Image.new('RGBA',(128,128),(0,0,0,255));background.alpha_composite(image);background.alpha_composite(radar);image=background
        health.paste(image.resize((256,256),Image.Resampling.NEAREST),(col*256,32));hd.text((col*256+8,10),f'Native indexed CLUT fill {fill}/253',fill='white')
    health.save(a.out/'native-health-indexed.png')
    (a.out/'provenance.json').write_text(json.dumps({'source_pack':str(a.assets.resolve()),'region':ext['region'],'type':'Offline original mesh/texture rasterization plus diagnostic geometry, not gameplay; no AI','files':['native-nozzle-mounts.png','native-health-indexed.png']},indent=2)+'\n')
    print(a.out)

if __name__=='__main__':main()
