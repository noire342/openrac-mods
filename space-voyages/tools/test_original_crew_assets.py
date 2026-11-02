"""CPU contracts for the captured Original cockpit; no game or GPU execution."""
import copy,json,math,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import original_crew_assets as crew
from asset_formats import digest
from build_assets import Source

WORK=Path(__file__).resolve().parents[3]
BASE=WORK/'space-voyages-assets-pal-flightfx-06'
CAPTURE=WORK/'veldin-ship/qa-menu-cockpit-11402-41/cockpit-source.json'
ISO=WORK/'space-pal-runtime-data-01/iso_data/rac1/disc.iso'
EDITOR=WORK/'OpenRAC-space-transit/editor'
LEVELS=WORK/'space-pal-data-01'
I=[1.,0,0,0,0,1.,0,0,0,0,1.,0,0,0,0,1.]

class PureContracts(unittest.TestCase):
    def test_vertex_contract_rejects_indices_nonfinite_colour_normal_and_envelope(self):
        v=(.1,.2,.3,0,0,1,0,0,.5,.5,.5,1)
        crew.valid_mesh([v],[0,0,0])
        for vertices,indices in [([],[]),([v],[0,0]),([v],[0,1,0]),
            ([(*v[:3],0,0,0,*v[6:])],[0,0,0]),
            ([(2,0,0,*v[3:])],[0,0,0]),
            ([(math.nan,*v[1:])],[0,0,0]),
            ([(*v[:8],3,*v[9:])],[0,0,0])]:
            with self.subTest(vertices=vertices,indices=indices):
                with self.assertRaises(ValueError):crew.valid_mesh(vertices,indices)

    def test_output_never_overwrites_existing_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'keep').write_bytes(b'immutable')
            with self.assertRaises(ValueError):crew.build(root/'missing',root/'missing',root/'missing',root/'missing',root,root/'missing')
            self.assertEqual((root/'keep').read_bytes(),b'immutable')

    def test_capture_pin_checked_before_native_source_is_read(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'capture.json';path.write_bytes(b'{}')
            with self.assertRaisesRegex(ValueError,'hash'):crew.captured_crew(None,None,None,path)

    def test_invalid_native_sample_context_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'capture.json';raw=b'{}';path.write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'boarding frame'):crew.captured_crew(None,None,None,path,digest(raw))

    def test_skin_uses_plain_parent_and_post_helper_without_double_inheritance(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);model=root/'mobys/a.glb';model.parent.mkdir();(model.parent/'texture').write_bytes(b'x')
            doc={'skins':[{'joints':[0,1],'inverseBindMatrices':0}],
                 'nodes':[{'name':'joint_000'},{'name':'joint_000_post'}],
                 'meshes':[{'primitives':[{'attributes':{'POSITION':1,'NORMAL':2,'TEXCOORD_0':3,'JOINTS_0':4,'WEIGHTS_0':5,'COLOR_0':6},'material':0,'indices':7}]}],
                 'materials':[{'name':'moby_fixture','pbrMetallicRoughness':{'baseColorTexture':{'index':0}}}],
                 'textures':[{'source':0}],'images':[{'uri':'texture'}]}
            arrays={0:[I,I],1:[(.1,0,0),(.1,0,0)],2:[(0,0,1)]*2,3:[(0,0)]*2,
                    4:[(0,0,0,0),(1,0,0,0)],5:[(1,0,0,0)]*2,6:[(.5,.6,.7,1)]*2,7:[(0,),(1,),(0,)]}
            post=list(I);post[0]=post[5]=post[10]=2
            with patch.object(crew,'glb_read',return_value=(doc,b'')),patch.object(crew,'values',side_effect=lambda d,r,i:arrays[i]):
                parts=crew.skin_actor(model,[I],[post])
                self.assertAlmostEqual(parts[0]['vertices'][0][0],.1)
                self.assertAlmostEqual(parts[0]['vertices'][1][0],.2)
                self.assertEqual(parts[0]['vertices'][0][8:],(.5,.6,.7,1))
                with self.assertRaisesRegex(ValueError,'plain post-scale parent'):crew.skin_actor(model,[post],[post],captured_post=True)

    def test_scene_sample_bounds_rejected_before_read(self):
        for bank,tick in [(-1,464),(10,464),(1,-1),(1,598)]:
            with self.assertRaises(ValueError):crew.scene_chunk(None,bank,tick)

@unittest.skipUnless(BASE.is_dir() and CAPTURE.is_file() and ISO.is_file(),'private native inputs are unavailable')
class NativeInputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture_before=digest(CAPTURE.read_bytes());cls.original=json.loads((BASE/'original-ship-extension.json').read_text())
        source=Source(ISO,EDITOR,'pal')
        try:cls.parts,cls.facts=crew.captured_crew(source,LEVELS,cls.original,CAPTURE)
        finally:source.close()

    def test_actual_capture_identity_and_native_palette_counts(self):
        self.assertEqual(self.capture_before,crew.CAPTURE_SHA256)
        self.assertEqual(set(self.parts),{0,10})
        self.assertEqual([self.facts['actors'][str(c)]['native_joints'] for c in (0,10)],[111,75])
        self.assertEqual([len(self.parts[c]) for c in (0,10)],[4,3])
        self.assertAlmostEqual(self.facts['actors']['0']['native_scale_ratio'],.4375,places=6)
        self.assertAlmostEqual(self.facts['actors']['10']['native_scale_ratio'],.625,places=6)

    def test_captured_mesh_materials_and_bounds_remain_valid(self):
        for parts in self.parts.values():
            for part in parts:crew.valid_mesh(part['vertices'],part['indices'])
        self.assertEqual(sum(len(p['vertices']) for parts in self.parts.values() for p in parts),10096)
        self.assertEqual(sum(len(p['indices'])//3 for parts in self.parts.values() for p in parts),11994)
        self.assertEqual(sum(p['sphere_uv'] for parts in self.parts.values() for p in parts),1)

    def test_canopy_fit_translates_without_rescaling_or_reposing(self):
        parts=copy.deepcopy(self.parts);facts=copy.deepcopy(self.facts)
        crew.fit_cockpit(parts,BASE,facts,self.original['scale'])
        self.assertLessEqual(facts['canopy_fit']['max_roof_overflow'],-.0039)
        for c in parts:
            delta=facts['actors'][str(c)]['fit_shift_space']
            for oldpart,newpart in zip(self.parts[c],parts[c]):
                for old,new in zip(oldpart['vertices'],newpart['vertices']):
                    for k in range(3):self.assertAlmostEqual(new[k]-old[k],delta[k],places=6)
                    self.assertEqual(old[3:],new[3:])
                crew.valid_mesh(newpart['vertices'],newpart['indices'])
        self.assertEqual(self.capture_before,digest(CAPTURE.read_bytes()))

if __name__=='__main__':unittest.main()
