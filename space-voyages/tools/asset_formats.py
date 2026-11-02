"""Bounded local-art readers; no dependency on the game runtime or renderer.

GLB layout follows glTF 2.0. RAC1 PIF/overlay layouts follow OpenRAC's GPL
readers in port/assets/world/{pif,font}. No game artwork is included here.
"""
from __future__ import annotations
import copy,json,math,struct
from pathlib import Path
from PIL import Image

FMT={5120:'b',5121:'B',5122:'h',5123:'H',5125:'I',5126:'f'}
COMP={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}

def digest(data):
    import hashlib
    return hashlib.sha256(data).hexdigest()

def bounded(data,offset,size):
    if offset<0 or size<0 or offset+size>len(data):raise ValueError('Source range outside its buffer')
    return data[offset:offset+size]

def pif(data):
    if data[:4]!=b'2FIP':raise ValueError('Expected a native PIF image')
    w,h,kind=struct.unpack_from('<III',data,8)
    if kind!=0x13 or not(0<w<=1024 and 0<h<=1024):raise ValueError('Unsupported native PIF')
    return indexed(bounded(data,0x420,w*h),bounded(data,0x20,1024),w,h)

def indexed(pixels,palette,w,h):
    if len(pixels)!=w*h or len(palette)!=1024:raise ValueError('Invalid indexed image')
    colours=[]
    for i in range(256):
        j=(i&~24)|((i&8)<<1)|((i&16)>>1)
        r,g,b,a=palette[j*4:j*4+4]
        colours.append(bytes((r,g,b,min(255,a*2))))
    return Image.frombytes('RGBA',(w,h),b''.join(colours[i] for i in pixels))

def glb_read(path):
    data=path.read_bytes()
    if len(data)<28 or struct.unpack_from('<III',data)!=(0x46546c67,2,len(data)):raise ValueError('Invalid GLB')
    n,kind=struct.unpack_from('<II',data,12)
    if kind!=0x4e4f534a:raise ValueError('GLB missing JSON')
    doc=json.loads(bounded(data,20,n));offset=20+n
    n,kind=struct.unpack_from('<II',data,offset)
    if kind!=0x004e4942 or offset+8+n!=len(data):raise ValueError('Invalid GLB binary')
    return doc,bounded(data,offset+8,n)

def values(doc,data,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    if a.get('sparse') or v.get('buffer',0)!=0:raise ValueError('Sparse/external accessor not supported')
    fmt='<'+FMT[a['componentType']]*COMP[a['type']];width=struct.calcsize(fmt)
    stride=v.get('byteStride',width);relative=a.get('byteOffset',0)
    if stride<width:raise ValueError('Invalid GLB stride')
    bounded(data,v.get('byteOffset',0),v['byteLength'])
    count=a['count'];end=relative+(count-1)*stride+width if count else relative
    if relative<0 or end>v['byteLength']:raise ValueError('GLB accessor outside buffer view')
    result=[struct.unpack_from(fmt,data,v.get('byteOffset',0)+relative+i*stride) for i in range(count)]
    if a.get('normalized') and a['componentType']!=5126:
        signed=a['componentType'] in (5120,5122)
        maximum={5120:127,5121:255,5122:32767,5123:65535,5125:4294967295}[a['componentType']]
        result=[tuple(max(-1.,x/maximum) if signed else x/maximum for x in value) for value in result]
    return result

def identity():return [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def mul(a,b):return [sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]
def transform(m,p,w=1):return tuple(sum(m[k*4+r]*(p[k] if k<3 else w) for k in range(4)) for r in range(3))
def normalize(v):
    n=math.sqrt(sum(x*x for x in v))
    return tuple(x/n for x in v) if n>1e-12 else (0,0,1)

def trs(node):
    if 'matrix' in node:return node['matrix']
    x,y,z,w=node.get('rotation',[0,0,0,1]);norm=math.sqrt(x*x+y*y+z*z+w*w)
    if not math.isfinite(norm) or norm<1e-8:raise ValueError('Invalid joint quaternion')
    x,y,z,w=(q/norm for q in (x,y,z,w));sx,sy,sz=node.get('scale',[1,1,1]);tx,ty,tz=node.get('translation',[0,0,0])
    return [(1-2*y*y-2*z*z)*sx,(2*x*y+2*z*w)*sx,(2*x*z-2*y*w)*sx,0,
            (2*x*y-2*z*w)*sy,(1-2*x*x-2*z*z)*sy,(2*y*z+2*x*w)*sy,0,
            (2*x*z+2*y*w)*sz,(2*y*z-2*x*w)*sz,(1-2*x*x-2*y*y)*sz,0,tx,ty,tz,1]

def bake_ship(path,sequence=1):
    doc,data=glb_read(path);nodes=copy.deepcopy(doc['nodes'])
    if doc.get('animations'):
        anim=doc['animations'][min(sequence,len(doc['animations'])-1)]
        for channel in anim['channels']:
            sampler=anim['samplers'][channel['sampler']]
            # Native space drawing selects sequence1/frame0. These are sampled
            # static ship records; subsequent movement belongs to the controller.
            target=channel['target'];out=values(doc,data,sampler['output'])
            if out:nodes[target['node']][target['path']]=list(out[0])
    parent={c:i for i,n in enumerate(nodes) for c in n.get('children',[])};cache={}
    def world(i):
        if i not in cache:cache[i]=mul(world(parent[i]),trs(nodes[i])) if i in parent else trs(nodes[i])
        return cache[i]
    skin=doc.get('skins',[None])[0]
    palette=[mul(world(joint),matrix) for joint,matrix in zip(skin['joints'],values(doc,data,skin['inverseBindMatrices']))] if skin else None
    meshes=[]
    for node_id,node in enumerate(nodes):
        if 'mesh' not in node:continue
        for prim in doc['meshes'][node['mesh']]['primitives']:
            if prim.get('mode',4)!=4:raise ValueError('Ship mesh must use triangles')
            attrs=prim['attributes'];positions=values(doc,data,attrs['POSITION'])
            normals=values(doc,data,attrs['NORMAL']) if 'NORMAL' in attrs else [(0,0,1)]*len(positions)
            texcoord=values(doc,data,attrs['TEXCOORD_0']) if 'TEXCOORD_0' in attrs else [(0,0)]*len(positions)
            joints=values(doc,data,attrs['JOINTS_0']) if 'JOINTS_0' in attrs else None
            weights=values(doc,data,attrs['WEIGHTS_0']) if 'WEIGHTS_0' in attrs else None
            baked=[]
            for k,(p,n,uv) in enumerate(zip(positions,normals,texcoord)):
                if joints and weights and palette:
                    pp=[transform(palette[j],p) for j in joints[k]];nn=[transform(palette[j],n,0) for j in joints[k]]
                    p=tuple(sum(pp[q][c]*weights[k][q] for q in range(4)) for c in range(3))
                    n=normalize(tuple(sum(nn[q][c]*weights[k][q] for q in range(4)) for c in range(3)))
                else:p=transform(world(node_id),p);n=normalize(transform(world(node_id),n,0))
                baked.append((*p,*n,*uv))
            indices=[int(x[0]) for x in values(doc,data,prim['indices'])] if 'indices' in prim else list(range(len(positions)))
            material=doc['materials'][prim.get('material',0)]
            texture_index=material['pbrMetallicRoughness']['baseColorTexture']['index']
            image=doc['images'][doc['textures'][texture_index]['source']]
            if 'uri' not in image:raise ValueError('Embedded ship texture needs an explicit extractor')
            texture=(path.parent/image['uri']).resolve()
            meshes.append((baked,indices,texture,material.get('alphaMode','OPAQUE')))
    if not meshes:raise ValueError('Ship contains no mesh')
    # Extracted RAC1 ships point along +X and use Z up. Normalize the entire
    # craft to a 2.5m length, preserving proportions and the sampled rig pose.
    all_positions=[v[:3] for vertices,_,_,_ in meshes for v in vertices]
    lo=[min(p[k] for p in all_positions) for k in range(3)];hi=[max(p[k] for p in all_positions) for k in range(3)]
    if not all(math.isfinite(x) for x in lo+hi) or max(hi[k]-lo[k] for k in range(3))>500:raise ValueError('Ship rig produced implausible geometry')
    scale=2.5/(hi[0]-lo[0]);centre=[(hi[k]+lo[k])/2 for k in range(3)]
    result=[]
    for vertices,indices,texture,alpha in meshes:
        result.append(([(*( (v[k]-centre[k])*scale for k in range(3)),*v[3:]) for v in vertices],indices,texture,alpha))
    return result,{'source_sequence':sequence,'forward_axis':'+X','up_axis':'+Z','length_metres':2.5,'source_bounds':[lo,hi],'scale':scale}

def sphere(longitudes=64,latitudes=32):
    vertices=[];indices=[]
    for row in range(latitudes+1):
        v=row/latitudes;lat=math.pi*(.5-v)
        for col in range(longitudes+1):
            u=col/longitudes;lon=(u-.5)*math.tau
            n=(math.cos(lat)*math.cos(lon),math.cos(lat)*math.sin(lon),math.sin(lat))
            vertices.append((*n,*n,u,v))
    for row in range(latitudes):
        for col in range(longitudes):
            a=row*(longitudes+1)+col;b=a+longitudes+1
            if row>0:indices.extend((a,b,a+1))
            if row<latitudes-1:indices.extend((a+1,b,b+1))
    return vertices,indices

def sphere_surface(preview,width=512,height=256):
    """Project the authentic view onto a sphere; far side is reconstructed.

    The original has one lit hemisphere, not a 360-degree surface. Sampling
    its central disk across a sphere preserves its colours and landmarks;
    opposite longitudes mirror that source. This is explicitly derived art.
    """
    preview=preview.convert('RGBA');w,h=preview.size;radius=min(w,h)*.445;cx=(w-1)/2;cy=(h-1)/2
    src=preview.load();pixels=[]
    for y in range(height):
        lat=math.pi*(.5-(y+.5)/height)
        for x in range(width):
            lon=((x+.5)/width-.5)*math.tau
            px=max(0,min(w-1,round(cx+math.sin(lon)*math.cos(lat)*radius)))
            py=max(0,min(h-1,round(cy-math.sin(lat)*radius)))
            r,g,b,_=src[px,py];pixels.append((r,g,b,255))
    out=Image.new('RGBA',(width,height));out.putdata(pixels);return out
