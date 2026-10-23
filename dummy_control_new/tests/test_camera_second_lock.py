from pathlib import Path
import unittest
import numpy as np
from camera_mode_core import CameraCapture,load_calibration


def line(frame,xyz):
    return f'XYZ RIGHT_RECT frame={frame:08X} valid=1 uv=(617,247) d_q4=1808 X_mm={xyz[0]} Y_mm={xyz[1]} Z_mm={xyz[2]} n=121/121 support=1000/1000 iqr_q4=16 calc_us=30000'


class SecondLockTests(unittest.TestCase):
    def make(self):
        c=CameraCapture(load_calibration(Path(__file__).resolve().parents[1]/'camera_calibration.json'))
        c.rearm_after(10)
        return c

    def test_fixed_target_experiment_converts_first_out_of_range_frame(self):
        c=self.make();c.rearm_after(10,force_single=True)
        bad=(469.893833,79.062,891.388)
        c.feed(line(0,bad),9,11) # Buffered frame from the first cycle.
        self.assertIsNone(c.locked)
        c.feed('XYZ RIGHT_RECT frame=00000001 valid=0 reason=GLOBAL_INVALID',11,11)
        self.assertIsNone(c.locked)
        # Window-quality checks must not block a fixed-target experiment either.
        packet=line(2,bad).replace('support=1000/1000','support=0/1000')
        c.feed(packet,11.1,11.1)
        self.assertEqual(c.locked,bad);self.assertTrue(c.frozen)
        expected=np.asarray(c.cal['R'])@np.array([bad[0],bad[1],bad[2]+20])+c.cal['t']+[10,0,0]
        np.testing.assert_allclose(c.check(11.1),expected)
        c.feed(line(3,(-5,-38,406)),12,12)
        self.assertEqual(c.locked,bad)
        c.reset();self.assertFalse(c.force_single)

    def test_reported_bad_lock_is_rejected_then_good_target_locks(self):
        c=self.make()
        for i in range(6):c.feed(line(i,(469.893833,79.062,891.388)),11+i*.6,11+i*.6)
        self.assertIsNone(c.locked);self.assertTrue(c.armed)
        self.assertEqual(len(c.samples),0)
        good=(1.712,-38.210,391.754)
        for i in range(5):c.feed(line(10+i,good),15+i*.6,15+i*.6)
        np.testing.assert_allclose(c.locked,good)
        self.assertTrue(c.frozen)
        np.testing.assert_allclose(c.check(17.5),np.asarray(c.cal['R'])@np.array([good[0],good[1],good[2]+20])+c.cal['t']+[10,0,0])

    def test_invalid_candidate_breaks_stable_sequence(self):
        c=self.make();good=(-5,-38,406)
        for i in range(3):c.feed(line(i,good),11+i*.6,11+i*.6)
        c.feed(line(3,(470,79,891)),13,13)
        for i in range(4):c.feed(line(4+i,good),14+i*.6,14+i*.6)
        self.assertIsNone(c.locked)
        c.feed(line(8,good),16.4,16.4)
        self.assertIsNotNone(c.locked)

    def test_frozen_target_survives_new_invalid_or_changed_coordinates(self):
        c=self.make();good=(-5,-38,406)
        for i in range(5):c.feed(line(i,good),11+i*.6,11+i*.6)
        target=c.check(13.5);sample=c.tracker.sample
        for packet in (line(6,(470,79,891)),line(7,(-44,-38,410)),
                       'XYZ RIGHT_RECT frame=00000008 valid=0 reason=GLOBAL_INVALID',
                       'STREAM STOP'):
            c.feed(packet,20,20)
        self.assertEqual(c.locked,good);self.assertEqual(c.target,target)
        self.assertIs(c.tracker.sample,sample)
        self.assertEqual(c.check(100),target) # New frame age/drift no longer controls this lock.
        c.used=True
        with self.assertRaises(ValueError):c.check(100)
        c.reset();self.assertFalse(c.frozen);self.assertIsNone(c.locked)

    def test_in_range_but_unstable_frames_do_not_freeze(self):
        c=self.make()
        for i in range(12):c.feed(line(i,(-40 if i%2 else -19,-38,406)),11+i*.6,11+i*.6)
        self.assertIsNone(c.locked);self.assertFalse(c.frozen)


if __name__=='__main__':unittest.main()
