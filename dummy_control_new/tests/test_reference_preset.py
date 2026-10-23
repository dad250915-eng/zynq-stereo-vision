import sys,unittest,time,math
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import main
from controller import Controller,angle_error
from kinematics import fk
from reference_preset import PRESET_XYZ,preset_solution

class PresetTests(unittest.TestCase):
    def ready(self):
        sent=[];c=Controller(sent.append,lambda _:None);c.enabled=True
        c.joints=(0,-75,180,0,0,0);c.pose=fk(c.joints);c.pose_at=c.joints_at=time.monotonic()
        app=SimpleNamespace(planning=False,sequence=None,require=lambda:c,log=lambda _:None,progress=SimpleNamespace(set=lambda _:None))
        app.camera_window=None
        app.check_camera_batch_idle=lambda:main.App.check_camera_batch_idle(app)
        return app,c,sent
    def test_source_endpoint_and_pinned_wrist(self):
        q,p=preset_solution()
        self.assertLess(math.dist(p[:3],(160,0,270)),.01)
        self.assertLess(angle_error(p[3:],(0,180,0)),.01)
        self.assertEqual((q[0],q[3],q[5]),(0.,0.,0.))
        self.assertAlmostEqual(q[4],90.362,delta=.01)
    def test_button_sends_one_joint_target_no_transition_or_grasp(self):
        app,c,sent=self.ready();main.App.reference_preset(app)
        self.assertEqual(len(sent),1);self.assertEqual(sent[0],'>0.000,3.271,86.367,0.000,90.362,0.000,30')
        self.assertEqual(c.target[:3],PRESET_XYZ)
        self.assertIsNone(app.sequence);self.assertFalse(app.planning)
        with self.assertRaises(ValueError):main.App.reference_preset(app)
        self.assertEqual(len(sent),1)
    def test_busy_disabled_stale_block(self):
        for state in ('planning','sequence','disabled','stale'):
            app,c,sent=self.ready()
            if state=='planning':app.planning=True
            elif state=='sequence':app.sequence=object()
            elif state=='disabled':c.enabled=False
            else:c.pose_at=0
            with self.assertRaises(ValueError):main.App.reference_preset(app)
            self.assertEqual(sent,[])
    def test_stop_cancels_preset(self):
        app,c,sent=self.ready();main.App.reference_preset(app)
        c.stop(time.monotonic());c.receive('Stopped ok',time.monotonic())
        self.assertIsNone(c.target);self.assertEqual(sent[-1],'!STOP')

if __name__=='__main__':unittest.main()
