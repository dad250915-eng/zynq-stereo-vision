import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import queue
import numpy as np
from mapped_controller import MappedController, map_joints
from controller import Controller
from kinematics import fk
from camera_mode_core import CameraCapture, load_calibration
from camera_pick_place import CameraBatch, CameraPickPlaceSequence, plan_pick_place, PLACEMENTS, RETRACTS
from camera_mode_ui import CameraWindow
from main import App


def camera_line(frame, xyz=(-5,-38,406)):
    return f'XYZ RIGHT_RECT frame={frame:08X} valid=1 uv=(454,313) d_q4=2640 X_mm={xyz[0]} Y_mm={xyz[1]} Z_mm={xyz[2]} n=121/121 support=1000/1000 iqr_q4=48 calc_us=1000'


def feedback(c,q,t):
    raw=map_joints(q)
    c.query=('joints',t);c.receive('ok '+' '.join(map(str,raw)),t+.01)
    c.query=('pose',t+.02);c.receive('ok '+' '.join(map(str,fk(raw))),t+.03)


class ImmediateThread:
    def __init__(self,target,daemon=True):self.target=target
    def start(self):self.target()


class PickPlaceTests(unittest.TestCase):
    def make_controller(self):
        sent=[];logs=[];c=MappedController(sent.append,logs.append);c.enabled=True
        feedback(c,(0,-75,180,0,0,0),100)
        return c,sent,logs

    def test_plan_holds_gripper_until_placement_and_lifts_open(self):
        for slot in (0,1):
            plan=plan_pick_place((200,0,160),(0,-75,180,0,0,0),slot)
            actions=plan['steps']
            hands=[(i,a['opening']) for i,a in enumerate(actions) if a['op']=='hand']
            self.assertEqual([opening for _,opening in hands], [True,False,True] if slot==0 else [False,True])
            close=next(i for i,a in enumerate(actions) if a['op']=='hand' and not a['opening'])
            release=hands[-1][0]
            self.assertEqual([a['pose'][:3] for a in actions[close+1:release]],[(160.,0.,270.),PLACEMENTS[slot]])
            self.assertTrue(all(a['op']=='move' for a in actions[release+1:]))
            self.assertEqual(len(actions[release+1:]),1)
            self.assertEqual(actions[-1]['speed'],30)
            np.testing.assert_allclose(actions[-1]['pose'][:3],RETRACTS[slot],atol=1e-10)

    def test_temporary_targets_replace_converted_coordinates_exactly(self):
        converted=(222.166,82.517,159.091)
        for slot,target in enumerate(((200.,0.,160.),(200.,-70.,160.))):
            plan=plan_pick_place(converted,(0,-75,180,0,0,0),slot)
            self.assertEqual(plan['converted_target'],converted)
            self.assertEqual(plan['target'],target)
            moves=[a for a in plan['steps'] if a['op']=='move']
            self.assertEqual(moves[1]['pose'][:3],target)
            np.testing.assert_allclose(fk(moves[1]['joints'])[:3],target,atol=.1)
            self.assertEqual(plan['placement'],PLACEMENTS[slot])
            self.assertEqual(plan['retract'],RETRACTS[slot])
        with patch('camera_pick_place.TEST_PICKUP_TARGETS',None):
            self.assertEqual(plan_pick_place(converted,(0,-75,180,0,0,0),0)['target'],converted)

    def test_physical_wire_sequence_and_arrival_gates(self):
        for slot in (0,1):
            c,sent,logs=self.make_controller()
            seq=CameraPickPlaceSequence(c,plan_pick_place((200,0,160),c.joints,slot),1)
            release_pose=None
            for i in range(1,600):
                t=100+i*.1
                feedback(c,c.joint_target if c.target is not None else c.joints,t)
                seq.tick(t+.04)
                if c.hand_queued:
                    logical=c.hand_queued
                    c.tick(t+.05)
                    if logical=='hand_open' and seq.steps[seq.index].get('goal'):
                        release_pose=c.pose[:3]
                        self.assertGreaterEqual(c.completed,4)
                    c.receive('ok hand close' if logical=='hand_open' else 'ok hand open',t+.06)
                if seq.done:break
            self.assertTrue(seq.done)
            # Physical open is HAND_C, physical close is HAND_O on this installation.
            self.assertEqual([s for s in sent if s.startswith('!HAND')],['!HAND_C','!HAND_O','!HAND_C'] if slot==0 else ['!HAND_O','!HAND_C'])
            np.testing.assert_allclose(release_pose,PLACEMENTS[slot],atol=.1)
            np.testing.assert_allclose(c.pose[:3],RETRACTS[slot],atol=.1)
            self.assertEqual(c.last_hand_ack,'hand_open')
            moves=[s for s in sent if s.startswith('>')]
            self.assertEqual(len(moves),5) # Reference, pickup, reference, drop, one retract.
            self.assertTrue(moves[-1].endswith(',30'))

    def test_no_ack_prevents_pickup(self):
        c,sent,_=self.make_controller()
        seq=CameraPickPlaceSequence(c,plan_pick_place((200,0,160),c.joints,0),1)
        seq.tick(100.1);c.tick(100.2)
        seq.tick(100.5)
        self.assertEqual(sent,['!HAND_C'])
        c.tick(103)
        with self.assertRaises(ValueError):seq.tick(103)
        self.assertFalse(any(s.startswith('>') for s in sent))

    def test_drop_not_confirmed_never_opens(self):
        c,sent,_=self.make_controller()
        plan=plan_pick_place((200,0,160),c.joints,0)
        seq=CameraPickPlaceSequence(c,plan,1)
        seq.index=next(i for i,a in enumerate(plan['steps']) if a['op']=='hand' and a.get('goal') and a['opening'])
        with self.assertRaises(ValueError):seq.tick(100.1)
        self.assertEqual(sent,[])

    def test_stop_and_recovery_do_not_advance(self):
        c,sent,_=self.make_controller();seq=CameraPickPlaceSequence(c,plan_pick_place((200,0,160),c.joints,0),1)
        c.recovery={'phase':'drain'};seq.tick(100.1);self.assertEqual(sent,[])
        c.recovery=None;c.stop(100.2)
        with self.assertRaises(ValueError):seq.tick(100.3)
        self.assertEqual(sent,['!STOP'])

    def test_cleared_hand_queue_aborts_without_retry(self):
        c,sent,_=self.make_controller();seq=CameraPickPlaceSequence(c,plan_pick_place((200,0,160),c.joints,0),1)
        seq.tick(100.1)
        self.assertEqual(c.hand_queued,'hand_open')
        c.hand_queued=None # Communication recovery cancels unsent control commands.
        with self.assertRaises(ValueError):seq.tick(100.2)
        self.assertEqual(sent,[])

    def make_window(self):
        c,sent,logs=self.make_controller()
        app=App.__new__(App);app.controller=c;app.link=SimpleNamespace(connected=True)
        app.test_window=None;app.plan_token=7;app.planning=False;app.sequence=None
        app.log=logs.append;app.progress=SimpleNamespace(set=lambda _:None)
        app.root=SimpleNamespace(bell=lambda:None);app.guard=lambda fn:fn()
        w=CameraWindow.__new__(CameraWindow);w.app=app;app.camera_window=w
        w.capture=CameraCapture(load_calibration(Path(__file__).resolve().parents[1]/'camera_calibration.json'))
        w.batch=CameraBatch();w.results=queue.Queue();w.events=queue.Queue();w.closed=False
        w.started_controller=None;w.started_token=None
        w.link=SimpleNamespace(connected=True,generation=1)
        w.allow=SimpleNamespace(get=lambda:True);w.speed=SimpleNamespace(get=lambda:'1')
        for name in ('live','fixed','compensated','target','status'):
            setattr(w,name,SimpleNamespace(set=lambda _:None))
        w.window=SimpleNamespace(bell=lambda:None,after=lambda *args:1)
        w.start=SimpleNamespace(configure=lambda **kw:None)
        return app,w,c,sent,logs

    def test_one_click_completes_two_cycles_with_new_acquisition_only(self):
        app,w,c,sent,logs=self.make_window()
        clock=[103.]
        with patch('camera_mode_ui.time.monotonic',side_effect=lambda:clock[0]),patch('camera_mode_ui.threading.Thread',ImmediateThread):
            for i in range(5):w.capture.feed(camera_line(i),100+i*.6,100+i*.6)
            feedback(c,c.joints,102.95);w.poll()
            self.assertEqual(sent,[]) # First lock never auto-starts.
            w.move();w.poll()
            self.assertEqual(w.batch.state,'running')
            self.assertEqual(app.sequence.plan['target'],(200.,0.,160.))
            self.assertEqual(app.sequence.plan['converted_target'],w.capture.target)
            self.assertNotEqual(w.capture.target,app.sequence.plan['target'])
            first_lift_at=None;new_frame=100
            for i in range(1,1600):
                clock[0]=103+i*.1;t=clock[0]-.04
                feedback(c,c.joint_target if c.target is not None else c.joints,t)
                app.tick_reference(clock[0])
                if c.hand_queued:
                    logical=c.hand_queued;c.tick(clock[0]+.001)
                    c.receive('ok hand close' if logical=='hand_open' else 'ok hand open',clock[0]+.002)
                if w.batch.state=='waiting_second':
                    if first_lift_at is None:
                        first_lift_at=clock[0]
                        self.assertIsNone(w.capture.locked)
                        np.testing.assert_allclose(c.pose[:3],RETRACTS[0],atol=.1)
                        # Buffered first-cycle frames cannot participate in round two.
                        for k in range(5):w.capture.feed(camera_line(50+k),first_lift_at-1,clock[0])
                        self.assertIsNone(w.capture.locked)
                    if clock[0]-first_lift_at>.5:
                        w.events.put(('FPGA_CAMERA',1,'line',camera_line(new_frame,(469.893833,79.062,891.388)),clock[0]))
                        new_frame+=1
                w.poll()
                if w.batch.slot==1 and app.sequence is not None:
                    self.assertEqual(app.sequence.plan['target'],(200.,-70.,160.))
                    self.assertEqual(app.sequence.plan['converted_target'],w.capture.target)
                if w.batch.state=='done':break
            self.assertEqual(w.batch.state,'done')
            np.testing.assert_allclose(c.pose[:3],RETRACTS[1],atol=.1)
            self.assertEqual([s for s in sent if s.startswith('!HAND')],['!HAND_C','!HAND_O','!HAND_C','!HAND_O','!HAND_C'])
            self.assertEqual(sum(s.startswith('CAMERA_MOVE ') for s in logs),2)
            self.assertTrue(w.capture.used)
            count=len(sent)
            for i in range(10):clock[0]+=.1;w.poll()
            self.assertEqual(len(sent),count) # No automatic third cycle.

    def test_stop_cancels_waiting_second_without_resume(self):
        app,w,c,sent,_=self.make_window()
        w.batch.begin(c,app.plan_token,1,1);w.batch.started(0)
        w.batch.completed(0,w.capture,101)
        app.stop()
        self.assertEqual(w.batch.state,'aborted');self.assertFalse(w.capture.armed)
        self.assertEqual(sent,['!STOP'])
        self.assertIsNone(app.sequence)

    def test_manual_motion_and_hand_blocked_while_waiting_second(self):
        app,w,c,sent,_=self.make_window()
        w.batch.begin(c,app.plan_token,1,1);w.batch.started(0);w.batch.completed(0,w.capture,101)
        for action in (lambda:app.hand_command(True),lambda:app.reference_preset(),lambda:app.queue_down((160,0,210),1)):
            with self.assertRaises(ValueError):action()
        self.assertEqual(sent,[])

    def test_changed_camera_connection_aborts_cycle(self):
        app,w,c,sent,_=self.make_window()
        w.batch.begin(c,app.plan_token,1,1);w.batch.started(0)
        w.link.generation=2;w.poll()
        self.assertEqual(w.batch.state,'aborted');self.assertEqual(sent,['!STOP'])


if __name__=='__main__':unittest.main()
