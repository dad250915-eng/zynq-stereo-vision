import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from parse_perf import parse
from prepare_board_config import prepare, symbols, MAP_BYTES
from submission_common import inventory, manifest_bytes
from check_submission import check

ROOT = Path(__file__).resolve().parents[2]


class PerfTests(unittest.TestCase):
    def test_concurrent_times_are_not_summed(self):
        r = parse('PERF frame=1 roi=10x10 units=us\n'
                  'PERF IP D=10 G=30 H=31 I=32 E=-1\n'
                  'PERF GHI=32 total=100 prep=15 present=2\n'
                  'PERF overlap_D_GHI=10 units=us mask_source=B\n'
                  'PERF CAPTURE direct=1 copy_us=0 round=200 units=us\n')
        self.assertEqual(r['rates']['roi_processing_hz'], 10000)
        self.assertEqual(r['rates']['complete_round_hz'], 5000)
        self.assertNotIn('ip_E', r['metrics'])
        self.assertEqual(r['metrics']['roi_GHI']['mean_us'], 32)

    def test_no_complete_round_invented(self):
        r = parse('PERF frame=1 roi=10x10 units=us\nPERF GHI=30 total=100 prep=15\n')
        self.assertNotIn('complete_round_hz', r['rates'])

    def test_unknown_units_and_full_modes_excluded(self):
        r = parse('PERF frame=1 units=ms\nPERF GHI=1 total=2\n'
                  'PERF DEPTH frame=2 total=200 units=us\n')
        self.assertEqual(r['frames'], 0)
        self.assertEqual(r['rates'], {})
        self.assertEqual(r['full_mode_lines_not_aggregated'], 1)

    def test_observed_excerpt_has_three_complete_rounds(self):
        r = parse((ROOT/'data/historical_serial_excerpt.txt').read_text(encoding='utf-8'))
        self.assertEqual(r['frames'], 3)
        self.assertEqual(r['metrics']['capture_round']['n'], 3)
        self.assertAlmostEqual(r['metrics']['capture_round']['mean_us'], (319971+319632+319754)/3)

    def test_full_mode_capture_cannot_overwrite_previous_roi(self):
        r = parse('PERF frame=1 units=us\nPERF GHI=20 total=100\n'
                  'PERF DEPTH frame=2 total=500 units=us\n'
                  'PERF CAPTURE round=1000 units=us\n')
        self.assertNotIn('complete_round_hz', r['rates'])
        self.assertEqual(r['metrics']['roi_total']['mean_us'],100)

    def test_capture_with_other_units_is_not_us(self):
        r = parse('PERF frame=1 units=us\nPERF GHI=20 total=100\n'
                  'PERF CAPTURE round=1000 units=ms\n')
        self.assertNotIn('complete_round_hz', r['rates'])


def make_elf(storage_size=MAP_BYTES*2, marker=0x101000):
    strings = bytearray(b'\0')
    syms = bytearray(16)
    for name,value,size in [('rectifyStorage',0x2000000,storage_size),
                            ('leftRectifyMap',0x1000000,4),
                            ('rightRectifyMap',0x1000004,4),
                            ('rectify_maps_wait_marker',marker,8)]:
        offset = len(strings)
        strings.extend(name.encode()+b'\0')
        syms.extend(struct.pack('<IIIBBH',offset,value,size,0,0,1))
    str_off = 52
    sym_off = str_off+len(strings)
    sh_off = sym_off+len(syms)
    ident = b'\x7fELF\x01\x01\x01'+bytes(9)
    h = struct.pack('<16sHHIIIIIHHHHHH',ident,2,40,1,0x100000,0,sh_off,0,52,0,0,40,3,0)
    sh = bytes(40)+struct.pack('<IIIIIIIIII',0,3,0,0,str_off,len(strings),0,0,1,0)
    sh += struct.pack('<IIIIIIIIII',0,2,0,0,sym_off,len(syms),1,0,4,16)
    return h+strings+syms+sh


class FileTests(unittest.TestCase):
    def setUp(self):
        base = ROOT/'build/tool_tests'
        base.mkdir(parents=True,exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.tmp.name).resolve()
        self.assertTrue(self.root.is_relative_to(base.resolve()))

    def tearDown(self):
        self.tmp.cleanup()

    def test_elf_pointer_symbol_is_not_array_buffer(self):
        elf=self.root/'test.elf';elf.write_bytes(make_elf())
        bit=self.root/'test.bit';bit.write_bytes(b'fixture-not-a-bitstream')
        init=self.root/'init.tcl';init.write_text('# fixture')
        left=self.root/'left.bin';left.write_bytes(bytes(MAP_BYTES))
        right=self.root/'right.bin';right.write_bytes(bytes(MAP_BYTES))
        r=prepare(elf,bit,init,left,right)
        self.assertEqual(r['left_addr'],0x2000000)
        self.assertEqual(r['left_ptr_symbol'],0x1000000)
        self.assertEqual(r['right_addr'],0x2000000+MAP_BYTES)
        self.assertNotEqual(r['left_addr'],r['left_ptr_symbol'])

    def test_bad_elf_rejected(self):
        p=self.root/'bad.elf';p.write_bytes(b'not an ELF')
        with self.assertRaises(ValueError):symbols(p)

    def test_text_line_endings_are_portable_but_binary_is_exact(self):
        p=self.root/'sample.v';p.write_bytes(b'module a;\r\nendmodule\r\n')
        before=inventory(self.root)['sample.v']
        p.write_bytes(b'module a;\nendmodule\n')
        self.assertEqual(before,inventory(self.root)['sample.v'])
        b=self.root/'map.bin';b.write_bytes(b'a\r\nb')
        self.assertEqual(manifest_bytes(b),b'a\r\nb')

    def test_short_map_rejected(self):
        p=self.root/'test.elf';p.write_bytes(make_elf())
        short=self.root/'short.bin';short.write_bytes(b'1234')
        with self.assertRaisesRegex(ValueError,'map size'):prepare(p,p,p,short,short)

    def test_bad_storage_layout_rejected(self):
        p=self.root/'test.elf';p.write_bytes(make_elf(storage_size=4))
        image=self.root/'image.bin';image.write_bytes(bytes(MAP_BYTES))
        with self.assertRaisesRegex(ValueError,'layout'):prepare(p,p,p,image,image)

    def test_hash_mismatch_and_unicode_name_detected(self):
        p=self.root/'hello.py';p.write_text('print(1)')
        known=inventory(self.root)
        (self.root/'SOURCE_MANIFEST.json').write_text(json.dumps({'files':known}))
        p.write_text('print(2)')
        (self.root/'说明.md').write_text('fixture',encoding='utf-8')
        result=check(self.root,True)
        self.assertTrue(any('Manifest changed: hello.py' in e for e in result['errors']))
        self.assertTrue(any('Non-English' in e for e in result['errors']))


if __name__=='__main__':unittest.main()
