#!/usr/bin/env python3
"""Offline textured views of the exported native mesh, NOT a game capture.

Optional NumPy is used only by this preview. The builder needs only Pillow.
"""
import argparse, json, math, struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

def render(vertices, indices, texture, azimuth, size=300):
    az=math.radians(azimuth);el=math.radians(23)
    eye=np.array((math.cos(az)*math.cos(el),math.sin(az)*math.cos(el),math.sin(el)))
    right=np.array((-math.sin(az),math.cos(az),0));up=np.cross(eye,right)
    position=vertices[:,:3];xy=np.column_stack((position@right,position@up))
    xy=xy*(size*.43)+size/2;xy[:,1]=size-xy[:,1]
    depth=position@eye;zbuffer=np.full((size,size),-np.inf)
    pixels=np.zeros((size,size,3),dtype=np.uint8);pixels[:]=(35,39,48)
    light=np.array((-.4,-.3,.85));light/=np.linalg.norm(light)
    h,w=texture.shape[:2]
    for ii in indices.reshape(-1,3):
        p=xy[ii];edge1=p[1]-p[0];edge2=p[2]-p[0]
        area=edge1[0]*edge2[1]-edge1[1]*edge2[0]
        if abs(area)<1e-9:continue
        lo=np.maximum(0,np.floor(p.min(axis=0)).astype(int));hi=np.minimum(size-1,np.ceil(p.max(axis=0)).astype(int))
        if np.any(lo>hi):continue
        yy,xx=np.mgrid[lo[1]:hi[1]+1,lo[0]:hi[0]+1];x=xx+.5;y=yy+.5
        b=((x-p[0,0])*(p[2,1]-p[0,1])-(y-p[0,1])*(p[2,0]-p[0,0]))/area
        c=((p[1,0]-p[0,0])*(y-p[0,1])-(p[1,1]-p[0,1])*(x-p[0,0]))/area;a=1-b-c
        zz=a*depth[ii[0]]+b*depth[ii[1]]+c*depth[ii[2]]
        sub=zbuffer[lo[1]:hi[1]+1,lo[0]:hi[0]+1];mask=(a>=-1e-9)&(b>=-1e-9)&(c>=-1e-9)&(zz>sub)
        if not mask.any():continue
        weights=np.stack((a[mask],b[mask],c[mask]),axis=-1)
        uv=weights@vertices[ii,6:8];rect=vertices[ii[0],12:16]
        st=rect[2:]+(uv-np.floor(uv))*rect[:2]
        tx=st[:,0]*w-.5;ty=st[:,1]*h-.5;ix=np.floor(tx).astype(int);iy=np.floor(ty).astype(int);fx=tx-ix;fy=ty-iy
        color=np.zeros((len(tx),3))
        for dx,dy,ww in ((0,0,(1-fx)*(1-fy)),(1,0,fx*(1-fy)),(0,1,(1-fx)*fy),(1,1,fx*fy)):
            color+=texture[np.clip(iy+dy,0,h-1),np.clip(ix+dx,0,w-1),:3]*ww[:,None]
        normals=weights@vertices[ii,3:6];normals/=np.maximum(np.linalg.norm(normals,axis=1)[:,None],1e-8)
        diffuse=np.maximum(normals@light,0);shade=.55+.45*diffuse
        color*=shade[:,None]*(weights@vertices[ii,8:11])
        pixels[yy[mask],xx[mask]]=np.clip(color,0,255).astype(np.uint8);sub[mask]=zz[mask]
    return Image.fromarray(pixels)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--assets',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);a=parser.parse_args()
    if a.out.exists():raise ValueError('Never overwrite an existing approval preview')
    manifest=json.loads((a.assets/'assets.json').read_text());entries={e['id']:e for e in manifest['assets']}
    sheet=Image.new('RGB',(900,680),(20,24,32));draw=ImageDraw.Draw(sheet)
    for row,name in enumerate(('asteroid_0','asteroid_1')):
        e=entries[name];vertices=np.array(list(struct.iter_unpack('<16f',(a.assets/e['vertices']).read_bytes())))
        indices=np.frombuffer((a.assets/e['indices']).read_bytes(),dtype='<u4')
        tex=np.asarray(Image.frombytes('RGBA',(e['width'],e['height']),(a.assets/e['rgba']).read_bytes()))
        for col,azimuth in enumerate((25,145,265)):
            sheet.paste(render(vertices,indices,tex,azimuth),(col*300,row*340+30))
            draw.text((col*300+8,row*340+8),f'{name} / native class {e["native_class"]} / {len(indices)//3} tris / {azimuth}deg',fill='white')
    a.out.parent.mkdir(parents=True,exist_ok=True);sheet.save(a.out)
    print(json.dumps({'output':str(a.out),'source':'Original exported geometry/textures, offline orthographic rasterization; not an in-game screenshot or AI image.'}))

if __name__=='__main__':main()
