"""Static native world assemblies for orbital destination art.

The port export already contains each terrain fragment and TIE class at LOD0.
We keep every terrain primitive and every placed TIE, including small details;
we deliberately never load Moby objects, gameplay scripts or the world's sky.
Each batch shares an exact texture and sampler, so tiled UVs remain untouched.
"""
from __future__ import annotations
from array import array
import json,math
from pathlib import Path
from PIL import Image
from asset_formats import digest,glb_read,identity,mul,normalize,transform,trs,values

ARTIFICIAL_DESTINATIONS=((6,'station_blarg'),(13,'station_gemlik'),(17,'fleet_drek'))

class Inputs:
    """Hash every decoded file, then verify that none changed during the build."""
    def __init__(self):self.files={}
    def read(self,path):
        path=Path(path).resolve();data=path.read_bytes();fingerprint=(len(data),digest(data))
        previous=self.files.setdefault(str(path),fingerprint)
        if previous!=fingerprint:raise ValueError(f'Source changed while reading: {path}')
        return data
    def check(self):
        for name,expected in self.files.items():
            data=Path(name).read_bytes()
            if (len(data),digest(data))!=expected:raise ValueError(f'Source changed during build: {name}')
        return {name:{'bytes':v[0],'sha256':v[1]} for name,v in sorted(self.files.items())}

def normal_transform(matrix,normal):
    """Inverse transpose of the linear part; works with non-uniform instances."""
    a,b,c=matrix[0:3];d,e,f=matrix[4:7];g,h,i=matrix[8:11]
    det=a*(e*i-f*h)-d*(b*i-c*h)+g*(b*f-c*e)
    if not math.isfinite(det) or abs(det)<1e-12:raise ValueError('Singular static instance matrix')
    x,y,z=normal
    sign=1 if det>0 else -1
    return normalize((sign*((e*i-f*h)*x+(c*h-b*i)*y+(b*f-c*e)*z),
                      sign*((f*g-d*i)*x+(a*i-c*g)*y+(c*d-a*f)*z),
                      sign*((d*h-e*g)*x+(b*g-a*h)*y+(a*e-b*d)*z)))

def determinant(matrix):
    a,b,c=matrix[0:3];d,e,f=matrix[4:7];g,h,i=matrix[8:11]
    return a*(e*i-f*h)-d*(b*i-c*h)+g*(b*f-c*e)

def instance_colour(packed):
    # PortLevelWriter's packed little-endian RGBA is LightTies' exact output.
    # Native modulation uses 0x80=1.0, including alpha. RGB can exceed 1.0.
    return tuple(((packed>>shift)&255)/128 for shift in (0,8,16,24))

def material_info(doc,path,material,inputs,texture_cache):
    mat=doc['materials'][material];pbr=mat.get('pbrMetallicRoughness',{})
    factor=tuple(pbr.get('baseColorFactor',(1,1,1,1)))
    if 'baseColorTexture' not in pbr:raise ValueError(f'Untextured native static material: {path}')
    tex=doc['textures'][pbr['baseColorTexture']['index']]
    image=doc['images'][tex['source']]
    if 'uri' not in image:raise ValueError('Embedded native texture unsupported')
    source=(path.parent/image['uri']).resolve()
    if source not in texture_cache:
        data=inputs.read(source)
        import io
        rgba=Image.open(io.BytesIO(data)).convert('RGBA')
        texture_cache[source]=(digest(rgba.tobytes()),rgba)
    fingerprint,rgba=texture_cache[source]
    sampler=doc.get('samplers',[{}])[tex.get('sampler',0)]
    wrap_s=sampler.get('wrapS',10497);wrap_t=sampler.get('wrapT',10497)
    if wrap_s not in (10497,33071) or wrap_t not in (10497,33071):raise ValueError('Unsupported original sampler')
    alpha=mat.get('alphaMode','OPAQUE')
    key=(fingerprint,rgba.width,rgba.height,wrap_s,wrap_t,alpha,mat.get('alphaCutoff',.5),mat.get('doubleSided',False))
    return key,rgba,factor,source

def read_model(path,inputs,texture_cache):
    """Decode each source primitive once, keeping attributes in original order."""
    inputs.read(path);doc,data=glb_read(path);nodes=doc.get('nodes',[])
    parents={child:i for i,node in enumerate(nodes) for child in node.get('children',[])};worlds={}
    def world(i):
        if i not in worlds:worlds[i]=mul(world(parents[i]),trs(nodes[i])) if i in parents else trs(nodes[i])
        return worlds[i]
    result=[]
    for ni,node in enumerate(nodes):
        if 'mesh' not in node:continue
        if 'skin' in node:raise ValueError('A static destination must not include a skinned actor')
        matrix=world(ni)
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            if primitive.get('mode',4)!=4:raise ValueError('Static mesh must use triangles')
            attrs=primitive['attributes'];positions=values(doc,data,attrs['POSITION']);n=len(positions)
            normals=values(doc,data,attrs['NORMAL']) if 'NORMAL' in attrs else [(0,0,1)]*n
            uv=values(doc,data,attrs['TEXCOORD_0']) if 'TEXCOORD_0' in attrs else [(0,0)]*n
            colours=values(doc,data,attrs['COLOR_0']) if 'COLOR_0' in attrs else [(1,1,1,1)]*n
            slots=values(doc,data,attrs['_LIGHT_SLOT']) if '_LIGHT_SLOT' in attrs else [(-1,)]*n
            indices=[int(v[0]) for v in values(doc,data,primitive['indices'])] if 'indices' in primitive else list(range(n))
            if len(indices)%3 or any(i<0 or i>=n for i in indices):raise ValueError('Invalid static triangle indices')
            if not all(len(attr)==n for attr in (normals,uv,colours,slots)):raise ValueError('Static attribute count mismatch')
            if determinant(matrix)<0:indices=[q for i in range(0,len(indices),3) for q in (indices[i],indices[i+2],indices[i+1])]
            vertices=[]
            for p,normal,st,colour,slot in zip(positions,normals,uv,colours,slots):
                colour=tuple(colour)+(1.,)*(4-len(colour))
                vertices.append((transform(matrix,p),normal_transform(matrix,normal),st,colour,int(slot[0])))
            key,image,factor,texture=material_info(doc,path,primitive.get('material',0),inputs,texture_cache)
            result.append((vertices,indices,key,image,factor,str(texture)))
    if not result:raise ValueError(f'Empty static native model: {path}')
    return result

def build_station(levels,level,name,out,write_mesh,inputs,region=None):
    root=levels/f'level_{level:02}'
    manifest=json.loads(inputs.read(root/'manifest.json'));placements=json.loads(inputs.read(root/'placements.json'))
    if manifest.get('level')!=level or placements.get('format')!=4:raise ValueError('Unexpected native static scene format')
    if region is not None and (manifest.get('version')!=region or manifest.get('game')!='rac1'):raise ValueError('Static scene region does not match the verified source disc')
    textures={};batches={};class_cache={};stats={'terrain_primitives':0,'tie_instances':0,'tie_classes':0,'mobys_excluded':len(placements.get('mobys',[])),'shrubs_excluded':len(placements.get('shrubs',[])),'tiny_ties_excluded':0,'lod':'exact exported LOD0; no simplification','normal_fallbacks':0}
    lo=[math.inf]*3;hi=[-math.inf]*3
    def append(model,matrix,light_colours=None):
        reflected=determinant(matrix)<0
        for vertices,indices,key,image,factor,texture in model:
            batch=batches.setdefault(key,{'vertices':array('f'),'indices':array('I'),'image':image,'sources':set(),'primitives':0})
            batch['sources'].add(texture);batch['primitives']+=1;base=len(batch['vertices'])//12
            for position,normal,uv,colour,slot in vertices:
                p=transform(matrix,position);n=normal_transform(matrix,normal)
                for k in range(3):lo[k]=min(lo[k],p[k]);hi[k]=max(hi[k],p[k])
                if light_colours is not None and slot>=0:
                    if not 0<=slot<len(light_colours):raise ValueError('TIE light slot outside its instance colour table')
                    c=instance_colour(light_colours[slot])
                else:c=colour
                c=tuple(c[k]*factor[k] for k in range(4))
                batch['vertices'].extend((*p,*n,*uv,*c))
            if reflected:batch['indices'].extend(base+q for i in range(0,len(indices),3) for q in (indices[i],indices[i+2],indices[i+1]))
            else:batch['indices'].extend(base+i for i in indices)
    terrain=read_model(root/manifest['terrain'],inputs,textures);stats['terrain_primitives']=len(terrain);append(terrain,identity());del terrain
    for placement in placements['ties']:
        class_id=placement['class']
        if class_id not in class_cache:
            class_cache[class_id]=read_model(root/manifest['classes']['tie'][str(class_id)]['mesh'],inputs,textures)
        colours=placement.get('colours')
        if colours is None or len(colours)!=64:raise ValueError('Native TIE missing its 64 original light colours')
        append(class_cache[class_id],placement['matrix'],colours);stats['tie_instances']+=1
    stats['tie_classes']=len(class_cache);del class_cache
    centre=[(lo[k]+hi[k])/2 for k in range(3)]
    radius=max(math.sqrt(sum((v[i+k]-centre[k])**2 for k in range(3))) for batch in batches.values() for v in (batch['vertices'],) for i in range(0,len(v),12))
    if not math.isfinite(radius) or radius<1:raise ValueError('Implausible original destination bounds')
    entries=[];source_materials=[]
    for part,(key,batch) in enumerate(batches.items()):
        vertices=batch['vertices']
        for i in range(0,len(vertices),12):
            for k in range(3):vertices[i+k]=(vertices[i+k]-centre[k])/radius
        asset_id=name+(f'_part{part}' if part else '')
        entry=write_mesh(out,asset_id,vertices,batch['indices'],batch['image'],stride=48)
        entry.update({'native_level':level,'part':part,'assembly':name,'wrap_s':key[3],'wrap_t':key[4],'alpha_mode':key[5],'alpha_cutoff':key[6],'double_sided':key[7]})
        entries.append(entry);source_materials.append({'asset':asset_id,'rgba_sha256':key[0],'wrap_s':key[3],'wrap_t':key[4],'source_textures':sorted(batch['sources']),'source_primitives':batch['primitives']})
    stats.update({'source_bounds':[lo,hi],'centre':centre,'radius':radius,'normalization':'(original world position - centre) / radius; maximum vertex radius = 1','batches':len(entries),'vertex_count':sum(e['vertex_count'] for e in entries),'triangle_count':sum(e['index_count']//3 for e in entries),'geometry_bytes':sum(e['vertex_count']*48+e['index_count']*4 for e in entries),'materials':source_materials,'scope':'all original terrain and all placed TIE objects; includes scenery rocks and disconnected authored structural pieces; no independent sky/celestial backdrop, no Moby, no enemy, no gameplay'} )
    return entries,stats
