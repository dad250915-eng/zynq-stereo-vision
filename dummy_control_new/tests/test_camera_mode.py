import sys,unittest,time,queue
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,'E:/dummy_control_new')
sys.path.insert(0,'E:/dummy_control_new/vendor')
from camera_mode_core import CameraCapture,load_calibration
from camera_mode_ui import CameraWindow
# Fixed legacy fixture keeps the +40 mm and existing capture regressions stable.
CAL=load_calibration(Path(__file__).resolve().parent/'fixtures/camera_calibration_20260929.json')
def line(frame,xyz=(-40,-20,300)):
    return f'XYZ RIGHT_RECT frame={frame:08X} valid=1 uv=(454,313) d_q4=2640 X_mm={xyz[0]} Y_mm={xyz[1]} Z_mm={xyz[2]} n=121/121 support=1000/1000 iqr_q4=48 calc_us=1000'
def ready():
    c=CameraCapture(CAL)
    for i in range(5):c.feed(line(i),i*.6,i*.6)
    return c
class Tests(unittest.TestCase):
    def test_stable_lock_and_frozen_value(self):
        c=ready();self.assertIsNotNone(c.locked);target=c.check(2.5)
        c.feed(line(6,(-39,-20,300)),3,3);self.assertEqual(target,c.target);self.assertEqual(c.locked,(-40,-20,300))
    def test_screenshot_jitter_and_extended_range(self):
        import numpy as np
        c=CameraCapture(CAL)
        center=np.array([-53.033,73.044,301.145])
        span=np.array([12.88,14.44,6.19])
        for i,k in enumerate((-.5,.5,-.25,.25,0)):
            c.feed(line(i,center+k*span),i*.6,i*.6)
        target=c.check(2.5)
        self.assertTrue(np.allclose(c.locked,center))
        self.assertTrue(np.allclose(target,[172.45408262891253, -34.20218199828325, 248.8400893127403]))
    def test_extended_range_still_bounded(self):
        c=CameraCapture(CAL)
        for i in range(5):c.feed(line(i,(-53,85.0,301)),i,i)
        with self.assertRaises(ValueError):c.check(4)
    def test_depth_offset_before_rotation_and_range(self):
        import numpy as np
        c=CameraCapture(CAL)
        for i in range(5):c.feed(line(i,(-55.4854,32.704,217.4286)),i,i)
        self.assertTrue(np.allclose(c.grasp_camera,(-55.4854,32.704,257.4286)))
        self.assertTrue(np.allclose(c.check(4),(174.5213052336721,51.375899388839284,212.59254811640184)))
        c.feed(line(6,(-55.4854,32.704,217.4286)),5,5)
        c.check(5) # Raw feedback is compared against the raw lock, not +40 mm.
        c.reset();self.assertIsNone(c.grasp_camera)
        for i in range(5):c.feed(line(i,(-55,32,340)),i,i)
        with self.assertRaisesRegex(ValueError,'深度补偿'):c.check(4)

    def test_duplicates_cannot_lock(self):
        c=CameraCapture(CAL)
        for i in range(20):c.feed(line(1),i,i)
        self.assertIsNone(c.locked)
    def test_jitter_cannot_lock(self):
        c=CameraCapture(CAL)
        for i in range(10):c.feed(line(i,(-40+21*(i%2),-20,330)),i,i)
        self.assertIsNone(c.locked)
    def test_stale_invalid_moved_and_used_block(self):
        for mode in ('stale','invalid','moved','used','restart'):
            c=ready()
            if mode=='invalid':c.feed('XYZ RIGHT_RECT valid=0',3,3)
            if mode=='moved':c.feed(line(8,(-61,-20,330)),3,3)
            if mode=='used':c.used=True
            if mode=='restart':c.feed(line(0),3,3)
            with self.assertRaises(ValueError):c.check(10 if mode=='stale' else 3)
    def test_backlog_and_reset(self):
        c=ready();c.reset();self.assertIsNone(c.locked)
        c.feed(line(10),1,10);self.assertIsNone(c.tracker.sample)
    def test_outside_calibration_range(self):
        c=CameraCapture(CAL)
        for i in range(5):c.feed(line(i,(-400,0,330)),i,i)
        with self.assertRaises(ValueError):c.check(4)
    def test_commit_one_shot_and_cancel(self):
        from controller import Controller
        from kinematics import fk
        from camera_pick_place import plan_pick_place as solve,CameraPickPlaceSequence,CameraBatch
        c=ready();now=time.monotonic();c.tracker.sample=__import__('dataclasses').replace(c.tracker.sample,received=now)
        sent=[];robot=Controller(sent.append,lambda _:None);robot.enabled=True
        robot.joints=(0,-65,160,0,0,0);robot.pose=fk(robot.joints);robot.joints_at=robot.pose_at=now
        # Use a synthetic reachable transformed target to test commit/one-shot gating.
        c.cal["coordinate_model"]="j5-reversed-v1"
        c.target=(170.,170.,210.)
        xyz=c.target;result=solve(xyz,robot.joints,0)
        app=SimpleNamespace(controller=robot,plan_token=2,planning=True,require=lambda:robot,log=lambda _:None,progress=SimpleNamespace(set=lambda _:None))
        def start_plan(owner,plan,baseline,speed):
            app.sequence=CameraPickPlaceSequence(owner,plan,speed);app.planning=True;app.sequence.tick(time.monotonic());owner.tick(time.monotonic())
        app.start_reference_plan=start_plan
        w=SimpleNamespace(app=app,capture=c,allow=SimpleNamespace(get=lambda:True),link=SimpleNamespace(connected=True,generation=1),started_controller=robot)
        w.batch=CameraBatch();w.batch.begin(robot,2,1,5)
        w.check_batch_session=lambda:CameraWindow.check_batch_session(w)
        w.abort_batch=lambda reason:CameraWindow.abort_batch(w,reason)
        item=(robot,1,1,xyz,robot.joints,5,result,None)
        CameraWindow.commit(w,item);self.assertEqual(sent,[])
        CameraWindow.commit(w,(robot,2,1,xyz,robot.joints,5,result,None));self.assertEqual(len(sent),1)
        with self.assertRaises(ValueError):CameraWindow.commit(w,(robot,2,1,xyz,robot.joints,5,result,None))
        self.assertEqual(len(sent),1)
if __name__=='__main__':unittest.main()
