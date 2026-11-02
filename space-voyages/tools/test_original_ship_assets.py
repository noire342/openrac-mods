"""Original identity/pose/importer contracts; no runtime or installed writes."""
import copy,json,math,shutil,struct,tempfile,unittest
from pathlib import Path
from original_ship_assets import (TARGET_RADIUS,IDENTITY,cruise_pose,hinge,
    transform,bake_original,checked_source,build,digest,file_inventory)

SOURCE=Path(__file__).resolve().parents[3]/'veldin-ship/generated-initial-ship-04'

def rig():
    nodes=[{'translation':[0,0,0]} for _ in range(32)]
    nodes[0]['children']=[1];nodes[1]['children']=[2];nodes[2]['children']=[3]
    nodes[3]['children']=[4,6,7,8,9,12,15,16,17,18,19,20,21,22,23,24,25]
    nodes[4]['children']=[5];nodes[6]['children']=[28];nodes[9]['children']=[10]
    nodes[10]['children']=[11];nodes[12]['children']=[13];nodes[13]['children']=[14]
    nodes[15]['children']=[29];nodes[24]['children']=[30];nodes[25]['children']=[26];nodes[26]['children']=[27]
    nodes[6]['rotation']=[0,math.sin(-.45),0,math.cos(-.45)]
    nodes[9]['translation']=[3,0,0];nodes[10]['translation']=[0,0,-1]
    nodes[16]['translation']=[4,0,0];nodes[20]['translation']=[4,0,1]
    raw=struct.pack('<4f',0,0,0,1)
    doc={'nodes':nodes,'skins':[{'joints':list(range(31))}],
         'buffers':[{'byteLength':16}],'bufferViews':[{'buffer':0,'byteLength':16}],
         'accessors':[{'bufferView':0,'componentType':5126,'count':1,'type':'VEC4'}],
         'animations':[{'name':'seq_00','samplers':[{'output':0}],
                        'channels':[{'sampler':0,'target':{'node':6,'path':'rotation'}}]}]}
    return doc,raw

class OriginalContracts(unittest.TestCase):
    def test_identity_never_claims_green(self):
        self.assertEqual(IDENTITY['garage_id'],0);self.assertEqual(IDENTITY['physical_class'],2047)
        self.assertEqual(IDENTITY['source_class'],530);self.assertEqual(IDENTITY['controller_class'],531)
        self.assertNotEqual(IDENTITY['physical_class'],IDENTITY['controller_class'])

    def test_hinge_preserves_own_pivot_and_distance(self):
        pivot=(2.3,-4,1.7);point=(3.2,1,-2);m=hinge(pivot,-1.3)
        for x,y in zip(transform(m,pivot),pivot):self.assertAlmostEqual(x,y,places=12)
        before=sum((x-y)**2 for x,y in zip(point,pivot))
        after=sum((x-y)**2 for x,y in zip(transform(m,point),pivot))
        self.assertAlmostEqual(before,after,places=11)

    def test_closed_reference_does_not_use_open_bind_or_seq1(self):
        doc,raw=rig();pose=cruise_pose(doc,raw)
        self.assertAlmostEqual(pose[6][0],1);self.assertAlmostEqual(pose[6][2],0)
        self.assertAlmostEqual(pose[6][8],0);self.assertAlmostEqual(pose[6][10],1)

    def test_legs_propagate_and_braces_use_leg_pivot(self):
        doc,raw=rig();pose=cruise_pose(doc,raw)
        self.assertAlmostEqual(pose[9][12],3)
        expected=transform(hinge((3,0,0),-1.3),(3,0,-1))
        for x,y in zip(pose[10][12:15],expected):self.assertAlmostEqual(x,y,places=12)
        expected=transform(hinge((3,0,0),-1.3),(4,0,0))
        for x,y in zip(pose[16][12:15],expected):self.assertAlmostEqual(x,y,places=12)
        self.assertNotEqual(pose[16][12:15],[4,0,0])

    def test_wrong_rig_missing_reference_and_cycles_rejected(self):
        doc,raw=rig();doc['skins'][0]['joints']=list(range(27))
        with self.assertRaises(ValueError):cruise_pose(doc,raw)
        doc,raw=rig();doc['animations'][0]['name']='seq_01_loop'
        with self.assertRaises(ValueError):cruise_pose(doc,raw)
        doc,raw=rig();doc['nodes'][30]['children']=[2]
        with self.assertRaises(ValueError):cruise_pose(doc,raw)

    def test_existing_output_remains_immutable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'keep').write_bytes(b'immutable')
            with self.assertRaises(ValueError):build(root/'missing',root/'missing',root)
            self.assertEqual((root/'keep').read_bytes(),b'immutable')

    @unittest.skipUnless(SOURCE.is_dir(),'Local private Original source is not installed')
    def test_real_source_closed_pose_materials_and_same_space_mounts(self):
        before=file_inventory(SOURCE);parts,info=bake_original(SOURCE)
        self.assertEqual(info['vertex_count'],4784);self.assertEqual(info['triangles'],4250)
        self.assertLessEqual(info['radius'],TARGET_RADIUS);self.assertGreater(info['radius'],1.44999)
        self.assertGreater(info['length'],1.79);self.assertLess(info['length'],1.81)
        self.assertEqual((parts[0]['alpha_mode'],parts[1]['alpha_mode']),('OPAQUE','BLEND'))
        self.assertFalse(parts[0]['sphere_uv']);self.assertTrue(parts[1]['sphere_uv'])
        self.assertTrue(all(v[8:]==(.5,.5,.5,1) for v in parts[1]['vertices']))
        self.assertTrue(all(v[6:8]==(.5,.5) for v in parts[1]['vertices']))
        self.assertEqual([m['mesh_joint'] for m in info['mounts']],[15,24])
        for mount in info['mounts']:
            self.assertEqual(mount['direction'],[-1.,0.,0.]);self.assertEqual(mount['ring_triangles'],48)
            anchor=[(v-c)*info['scale'] for v,c in zip(mount['source_attachment'],info['centre_source'])]
            self.assertLess(math.dist(anchor,mount['position']),.0002)
            self.assertGreater(mount['radius'],.22);self.assertLess(mount['radius'],.23)
        self.assertEqual(before,file_inventory(SOURCE))

    @unittest.skipUnless(SOURCE.is_dir(),'Local private Original source is not installed')
    def test_original_provenance_and_cpu_integrity_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'source';shutil.copytree(SOURCE,root)
            p=root/'initial-ship-provenance.json';meta=json.loads(p.read_text());meta['renderMeshSha256']='0'*64
            p.write_text(json.dumps(meta))
            with self.assertRaisesRegex(ValueError,'hash'):checked_source(root)
            meta['renderMeshSha256']=digest((root/'graphics/mobys/moby_2047.glb').read_bytes());p.write_text(json.dumps(meta))
            cpu=root/'resources/class-2047.bin';data=bytearray(cpu.read_bytes());data[-1]^=1;cpu.write_bytes(data)
            with self.assertRaisesRegex(ValueError,'integrity'):checked_source(root)

if __name__=='__main__':unittest.main()
