#!/usr/bin/env python3
"""Offline original-model port diagnostic; no runtime or AI imagery."""
import argparse,json,math,struct
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from preview_asteroids import render

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--assets',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    if a.out.exists():raise ValueError('Refusing to overwrite a preview')
    a.out.mkdir(parents=True);man=json.loads((a.assets/'assets.json').read_text());e=json.loads((a.assets/'weapons-extension.json').read_text())
    sheet=Image.new('RGB',(1350,1000),(20,24,32));draw=ImageDraw.Draw(sheet)
    draw.text((8,8),'OFFLINE native geometry. Cyan: nacelle aperture. Green: MOD emitter forward direction. Not native combat/gameplay.',fill='white')
    for row,cl in enumerate((532,533)):
        entry=next(q for q in man['assets'] if q['id']==f'ship_{cl}')
        vv=np.array(list(struct.iter_unpack('<8f',(a.assets/entry['vertices']).read_bytes())))
        vv=np.column_stack((vv,np.ones((len(vv),4)),np.tile([1,1,0,0],(len(vv),1))));vv[:,:3]*=.7
        ii=np.frombuffer((a.assets/entry['indices']).read_bytes(),dtype='<u4')
        tex=np.asarray(Image.frombytes('RGBA',(entry['width'],entry['height']),(a.assets/entry['rgba']).read_bytes()))
        for col,az in enumerate((0,45,315)):
            image=render(vv,ii,tex,az,450);d=ImageDraw.Draw(image)
            angle=math.radians(az);el=math.radians(23)
            eye=np.array((math.cos(angle)*math.cos(el),math.sin(angle)*math.cos(el),math.sin(el)))
            right=np.array((-math.sin(angle),math.cos(angle),0));up=np.cross(eye,right)
            def project(pt):
                pt=np.array(pt)*.7
                return float(pt@right*450*.43+225),float(450-(pt@up*450*.43+225))
            for i,m in enumerate(e['ships'][str(cl)]['muzzles']):
                pos=np.array(m['position']);r=m['aperture_radius']
                ring=[project(pos+[0,r*math.cos(j*math.tau/32),r*math.sin(j*math.tau/32)]) for j in range(33)]
                d.line(ring,fill='cyan',width=2);d.line((project(pos),project(pos+[.45,0,0])),fill=(80,255,100),width=2)
                x,y=project(pos);d.text((x+4,y+4),str(i),fill='cyan')
            sheet.paste(image,(450*col,475*row+50));draw.text((450*col+8,475*row+30),f'Ship{cl} / original surface / MOD gun function / az{az}',fill='white')
    sheet.save(a.out/'mod-nacelle-emitter-mounts.png')
    entry=next(q for q in man['assets'] if q['id']=='projectile_laser')
    vv=np.array(list(struct.iter_unpack('<12f',(a.assets/entry['vertices']).read_bytes())))
    vv=np.column_stack((vv,np.tile([1,1,0,0],(len(vv),1))))
    lo=vv[:,:3].min(axis=0);hi=vv[:,:3].max(axis=0);vv[:,:3]=(vv[:,:3]-(lo+hi)/2)/(hi[0]-lo[0])*1.8
    ii=np.frombuffer((a.assets/entry['indices']).read_bytes(),dtype='<u4');tex=np.asarray(Image.frombytes('RGBA',(entry['width'],entry['height']),(a.assets/entry['rgba']).read_bytes()))
    beam=Image.new('RGB',(900,330),(20,24,32));bd=ImageDraw.Draw(beam)
    for col,az in enumerate((45,90,270)):
        beam.paste(render(vv,ii,tex,az,300),(300*col,30));bd.text((300*col+5,8),f'NATIVE projectile1009 / 384tris / az{az}',fill='white')
    beam.save(a.out/'native-laser1009-mesh.png')
    (a.out/'provenance.json').write_text(json.dumps({'source_pack':str(a.assets.resolve()),'type':'Offline original-model diagnostic; ports repurposed as mod guns; no native armed-ship claim, no gameplay, no AI'},indent=2)+'\n')
    print(a.out)

if __name__=='__main__':main()
