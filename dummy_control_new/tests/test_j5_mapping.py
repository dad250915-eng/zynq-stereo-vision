import unittest
from mapped_controller import MappedController, map_joints
from kinematics import fk
from reference_preset import preset_solution

class MappingTests(unittest.TestCase):
    def setup_controller(self):
        sent=[];logs=[];c=MappedController(sent.append,logs.append);c.enabled=True
        q=(0,-59.9,161.97,0,17.44,0)
        for t in (.1,.4,.7):self.feed(c,q,t)
        return c,sent,logs,q

    def feed(self,c,q,t):
        c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.01)
        c.query=('pose',t+.02);c.receive('ok '+' '.join(map(str,fk(q))),t+.03)

    def test_real_photo_feedback(self):
        c,s,l,raw=self.setup_controller()
        self.assertEqual(c.joints[4],-17.44)
        self.assertAlmostEqual(c.pose[2],239.455,places=2)
        self.assertAlmostEqual(c.raw_pose[2],202.82,places=2)
        c.check_auto_ready(.8)

    def test_preset_wire_and_completion(self):
        c,s,l,raw=self.setup_controller();q,p=preset_solution()
        c.move_down(p[:3],q,p,c.joints,1,.8)
        self.assertEqual(s[-1],'>0.000,3.271,86.367,0.000,-90.362,0.000,1')
        for t in (1.,1.4,1.8):self.feed(c,map_joints(q),t)
        self.assertEqual(c.completed,1)
        self.assertAlmostEqual(c.pose[2],270,places=2)

    def test_old_positive_j5_cannot_complete(self):
        c,s,l,raw=self.setup_controller();q,p=preset_solution()
        c.move_down(p[:3],q,p,c.joints,1,.8)
        for t in (1.,1.4,1.8):self.feed(c,q,t)
        self.assertEqual(c.completed,0)

    def test_return_roundtrip(self):
        c,s,l,raw=self.setup_controller();c.record_return_pose(.8)
        changed=list(raw);changed[4]+=3;self.feed(c,changed,1.)
        c.return_to_recorded(1,1.1)
        self.assertEqual(s[-1],'>0.000,-59.900,161.970,0.000,17.440,0.000,1')

    def test_small_probe_corrected_direction(self):
        from j5_probe import start_probe
        c,s,l,raw=self.setup_controller();start_probe(c,.8)
        self.assertEqual(s[-1],'>0.000,-59.900,161.970,0.000,14.440,0.000,30')
        c.stop(.9);self.assertEqual(s[-1],'!STOP')

    def test_old_cartesian_command_blocked_and_stale(self):
        c,s,l,raw=self.setup_controller()
        with self.assertRaises(ValueError):c.tx('@160,0,270,0,180,0,1')
        with self.assertRaises(ValueError):c.check_auto_ready(10)
        self.assertEqual(s,[])

    def test_recovery_uses_raw_data(self):
        c,s,l,raw=self.setup_controller();c.bad_frame('test',.9)
        for i,v in enumerate((raw,raw,fk(raw),fk(raw))):
            c.recovery['phase']='probe';c.query=('joints' if i<2 else 'pose',1+i)
            c.receive('ok '+' '.join(map(str,v)),1.1+i)
        self.assertIsNone(c.recovery)
        with self.assertRaises(ValueError):c.check_auto_ready(4.2)
        self.feed(c,raw,5);c.check_auto_ready(5.1)

if __name__=='__main__':unittest.main()
