"""Region selection must reject mixed disc/export inputs before writing art."""
import json,tempfile,unittest
from pathlib import Path
from build_assets import match_source_region,validate_levels

class RegionAssets(unittest.TestCase):
    def test_explicit_and_detected_regions(self):
        for actual in ('ntsc','pal'):
            self.assertEqual(match_source_region('auto',actual),actual)
            self.assertEqual(match_source_region(actual,actual),actual)
        with self.assertRaises(ValueError):match_source_region('ntsc','pal')
        with self.assertRaises(ValueError):match_source_region('pal','ntsc')
    def test_unknown_regions_rejected(self):
        with self.assertRaises(ValueError):match_source_region('auto','jp')
        with self.assertRaises(ValueError):match_source_region('PAL','pal')
    def test_export_region_validation_includes_all_native_assemblies(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for level in (98,6,13,17):
                path=root/f'level_{level:02}';path.mkdir()
                (path/'manifest.json').write_text(json.dumps({'game':'rac1','version':'pal','level':level}))
            validate_levels(root,'pal',True)
            with self.assertRaises(ValueError):validate_levels(root,'ntsc',True)
            (root/'level_17/manifest.json').write_text(json.dumps({'game':'rac1','version':'ntsc','level':17}))
            validate_levels(root,'pal',False)
            with self.assertRaises(ValueError):validate_levels(root,'pal',True)

if __name__=='__main__':unittest.main()
