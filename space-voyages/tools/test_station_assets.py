"""Source-contract tests for faithful static assemblies, not game physics."""
from array import array
import json,math,struct,tempfile,unittest
from pathlib import Path
from PIL import Image
from asset_formats import normalize,transform,trs
from build_assets import write_mesh
from station_assets import Inputs,build_station,instance_colour,normal_transform,read_model

def fixture_glb(path,slot=False,wrap=10497):
    data=bytearray();views=[];accessors=[]
    def accessor(rows,kind):
        raw=b''.join(struct.pack('<'+'f'*len(row),*row) for row in rows)
        views.append({'buffer':0,'byteOffset':len(data),'byteLength':len(raw)});data.extend(raw)
        accessors.append({'bufferView':len(views)-1,'componentType':5126,'count':len(rows),'type':kind})
        return len(accessors)-1
    attributes={'POSITION':accessor([(0,0,0),(2,0,0),(0,2,0)],'VEC3'),
                'NORMAL':accessor([(0,0,1)]*3,'VEC3'),
                'TEXCOORD_0':accessor([(-2,3),(2,0),(0,2)],'VEC2')}
    if slot:attributes['_LIGHT_SLOT']=accessor([(2,)]*3,'SCALAR')
    else:attributes['COLOR_0']=accessor([(.5,1,1.5,1)]*3,'VEC4')
    doc={'asset':{'version':'2.0'},'bufferViews':views,'accessors':accessors,'buffers':[{'byteLength':len(data)}],
         'nodes':[{'mesh':0}],'meshes':[{'primitives':[{'attributes':attributes,'material':0}]}],
         'images':[{'uri':'texture.png'}],'textures':[{'source':0,'sampler':0}],
         'samplers':[{'wrapS':wrap,'wrapT':10497}],
         'materials':[{'doubleSided':True,'pbrMetallicRoughness':{'baseColorTexture':{'index':0}}}]}
    js=json.dumps(doc).encode();js+=b' '*((-len(js))%4);data+=b'\0'*((-len(data))%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(js)+len(data))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(data),0x004e4942)+data)

class StaticAssemblies(unittest.TestCase):
    def test_native_light_slot_colour_scale(self):
        self.assertEqual(instance_colour(128|(64<<8)|(255<<16)|(128<<24)),(1.,.5,255/128,1.))
    def test_normal_uses_inverse_transpose_not_inverse(self):
        matrix=trs({'rotation':[0,0,math.sin(.35),math.cos(.35)],'scale':[2,3,4]})
        tangent=transform(matrix,(1,1,0),0);normal=normal_transform(matrix,normalize((1,-1,0)))
        self.assertAlmostEqual(sum(tangent[i]*normal[i] for i in range(3)),0.,7)
        rotation=trs({'rotation':[0,0,math.sin(.35),math.cos(.35)]})
        self.assertAlmostEqual(normal_transform(rotation,(1,0,0))[1],math.sin(.7),7)
        self.assertEqual(normal_transform(trs({'scale':[-1,1,1]}),(1,0,0)),(-1.,0.,0.))
        with self.assertRaises(ValueError):normal_transform(trs({'scale':[1,0,1]}),(0,0,1))
    def test_full_static_merge_keeps_uv_and_instance_colour(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);root=base/'levels/level_06';root.mkdir(parents=True);out=base/'out';out.mkdir()
            Image.new('RGBA',(2,2),(128,128,128,255)).save(root/'texture.png')
            fixture_glb(root/'terrain.glb');fixture_glb(root/'tie.glb',True)
            (root/'manifest.json').write_text(json.dumps({'level':6,'terrain':'terrain.glb','classes':{'tie':{'7':{'mesh':'tie.glb'}}}}))
            colours=[0]*64;colours[2]=128|(64<<8)|(255<<16)|(128<<24)
            (root/'placements.json').write_text(json.dumps({'format':4,'ties':[{'class':7,'matrix':trs({'translation':[3,0,0]}),'colours':colours}],'mobys':[{'class':99}],'shrubs':[]}))
            inputs=Inputs();entries,info=build_station(base/'levels',6,'station_blarg',out,write_mesh,inputs)
            self.assertEqual(len(entries),1);self.assertEqual(info['tie_instances'],1);self.assertEqual(info['mobys_excluded'],1)
            entry=entries[0];vv=list(struct.iter_unpack('<12f',(out/entry['vertices']).read_bytes()))
            self.assertEqual(len(vv),6);self.assertEqual(vv[0][6:8],(-2.,3.))
            self.assertEqual(vv[0][8:],(.5,1.,1.5,1.));self.assertEqual(vv[3][8:],(1.,.5,255/128,1.))
            self.assertAlmostEqual(max(math.sqrt(sum(x*x for x in v[:3])) for v in vv),1.,6)
            self.assertEqual(entry['vertex_stride'],48);self.assertEqual(entry['wrap_s'],10497)
            self.assertEqual(len(inputs.check()),5)
            (root/'texture.png').write_bytes(b'changed')
            with self.assertRaises(ValueError):inputs.check()
    def test_new_vertex_writer_rejects_bad_stride_and_nonfinite(self):
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);image=Image.new('RGBA',(1,1))
            with self.assertRaises(ValueError):write_mesh(out,'bad',array('f',[0]*12),[0,0,0],image,stride=40)
            with self.assertRaises(ValueError):write_mesh(out,'bad',array('f',[math.nan]*12),[0,0,0],image,stride=48)
    def test_equal_texture_different_sampler_must_not_merge(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);Image.new('RGBA',(2,2),(255,255,255,255)).save(root/'texture.png')
            fixture_glb(root/'repeat.glb',wrap=10497);fixture_glb(root/'clamp.glb',wrap=33071)
            inputs=Inputs();cache={}
            repeat=read_model(root/'repeat.glb',inputs,cache)[0]
            clamp=read_model(root/'clamp.glb',inputs,cache)[0]
            self.assertEqual(repeat[2][0],clamp[2][0])
            self.assertNotEqual(repeat[2],clamp[2])
            self.assertEqual(repeat[0][0][2],(-2.,3.))
            self.assertEqual(clamp[0][0][2],(-2.,3.))

if __name__=='__main__':unittest.main()
